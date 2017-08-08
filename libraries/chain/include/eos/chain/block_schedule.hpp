/*
 * Copyright (c) 2017, Respective Authors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#include <eos/chain/global_property_object.hpp>
#include <eos/chain/transaction.hpp>

#include <random>
#include <set>

namespace eos { namespace chain {
   using pending_transaction = static_variant<SignedTransaction const *, GeneratedTransaction const *>;

   struct thread_schedule {
      vector<pending_transaction> transactions;
   };

   using cycle_schedule = vector<thread_schedule>;

   /**
    *   @class block_schedule
    *   @brief represents a proposed order of execution for a generated block
    */
   struct block_schedule
   {
      typedef block_schedule (*factory)(vector<pending_transaction> const &, const global_property_object&);
      vector<cycle_schedule> cycles;

      // Algorithms
      
      /**
       * A greedy scheduler that attempts to make short threads to resolve scope contention before 
       * falling back on cycles
       * @return the block scheduler
       */
      static block_schedule by_threading_conflicts(vector<pending_transaction> const &transactions, const global_property_object& properties);

      /**
       * A greedy scheduler that attempts uses future cycles to resolve scope contention
       * @return the block scheduler
       */
      static block_schedule by_cycling_conflicts(vector<pending_transaction> const &transactions, const global_property_object& properties);

      /*
       * templated meta schedulers for fuzzing
       */
      template<typename NEXT>
      struct shuffled_functor {
         shuffled_functor(NEXT &&_next) : next(_next){};

         block_schedule operator()(vector<pending_transaction> const &transactions, const global_property_object& properties) {
            std::random_device rd;
            std::mt19937 rng(rd());
            auto copy = std::vector<pending_transaction>(transactions);
            std::shuffle(copy.begin(), copy.end(), rng);
            return next(copy, properties);
         }

         NEXT &&next;
      };

      template<typename NEXT> 
      static shuffled_functor<NEXT> shuffled(NEXT &&next) {
         return shuffled_functor<NEXT>(next);
      }

      template<int NUM, int DEN, typename NEXT>
      struct lossy_functor {
         lossy_functor(NEXT &&_next) : next(_next){};
         
         block_schedule operator()(vector<pending_transaction> const &transactions, const global_property_object& properties) {
            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_real_distribution<> dist(0, 1);
            double const cutoff = (double)NUM / (double)DEN;

            auto copy = std::vector<pending_transaction>();
            copy.reserve(transactions.size());
            std::copy_if (transactions.begin(), transactions.end(), copy.begin(), [&](pending_transaction const& trx){
               return dist(rng) >= cutoff;
            });

            return next(copy, properties);
         }

         NEXT &&next;
      };

      template<int NUM, int DEN, typename NEXT> 
      static lossy_functor<NUM, DEN, NEXT> lossy(NEXT &&next) {
         return lossy_functor<NUM, DEN, NEXT>(next);
      }
   };

   struct scope_extracting_visitor : public fc::visitor<std::set<AccountName>> {
      template <typename T>
      std::set<AccountName> operator()(const T &trx_p) const {
         std::set<AccountName> unique_names(trx_p->scope.begin(), trx_p->scope.end());
         for (auto const &m : trx_p->messages) {
            unique_names.insert(m.code);
         }

         return unique_names;
      }
   };

} } // eos::chain

FC_REFLECT(eos::chain::thread_schedule, (transactions))
FC_REFLECT(eos::chain::block_schedule, (cycles))
