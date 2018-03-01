/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utility>

#include "ordering/impl/ordering_gate_impl.hpp"

namespace iroha {
  namespace ordering {

    OrderingGateImpl::OrderingGateImpl(
        std::shared_ptr<iroha::network::OrderingGateTransport> transport)
        : transport_(std::move(transport)), log_(logger::log("OrderingGate")) {}

    void OrderingGateImpl::propagateTransaction(
        std::shared_ptr<const model::Transaction> transaction) {
      log_->info("propagate tx, tx_counter: "
                 + std::to_string(transaction->tx_counter)
                 + " account_id: " + transaction->creator_account_id);

      transport_->propagateTransaction(transaction);
    }

    rxcpp::observable<model::Proposal> OrderingGateImpl::on_proposal() {
      return proposals_.get_observable();
    }

    bool OrderingGateImpl::setPcs(
        std::weak_ptr<iroha::network::PeerCommunicationService> psc) {
      if (psc.expired())
        return false;
      psc_subscriber_ = psc.lock()->on_commit().subscribe([this](auto) {
        unlock_next_.store(true);
        tryNextRound();

      });
      return true;
    }

    void OrderingGateImpl::onProposal(model::Proposal proposal) {
      log_->info("Received new proposal");
      proposal_queue_.push(std::make_shared<model::Proposal>(proposal));
      tryNextRound();
    }

    void OrderingGateImpl::tryNextRound() {
      std::shared_ptr<model::Proposal> next_proposal;
      if (unlock_next_.load() and proposal_queue_.try_pop(next_proposal)) {
        unlock_next_.store(false);
        proposals_.get_subscriber().on_next(*next_proposal);
      }
    }

    OrderingGateImpl::~OrderingGateImpl() {
      psc_subscriber_.unsubscribe();
    }

  }  // namespace ordering
}  // namespace iroha
