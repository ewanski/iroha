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

#include "ametsuchi/impl/postgres_wsv_query.hpp"

#include "common/result.hpp"
#include "model/account.hpp"
#include "model/account_asset.hpp"
#include "model/asset.hpp"
#include "model/domain.hpp"
#include "model/peer.hpp"

namespace iroha {
  namespace ametsuchi {

    const std::string kRoleId = "role_id";
    const char *kAccountNotFound = "Account {} not found";
    const std::string kPublicKey = "public_key";
    const std::string kAssetId = "asset_id";
    const std::string kAccountId = "account_id";
    const std::string kDomainId = "domain_id";

    PostgresWsvQuery::PostgresWsvQuery(pqxx::nontransaction &transaction)
        : transaction_(transaction),
          log_(logger::log("PostgresWsvQuery")),
          execute_{makeExecute(transaction_, log_)} {}

    bool PostgresWsvQuery::hasAccountGrantablePermission(
        const std::string &permitee_account_id,
        const std::string &account_id,
        const std::string &permission_id) {
      return execute_(
                 "SELECT * FROM account_has_grantable_permissions WHERE "
                 "permittee_account_id = "
                 + transaction_.quote(permitee_account_id)
                 + " AND account_id = " + transaction_.quote(account_id)
                 + " AND permission_id = " + transaction_.quote(permission_id)
                 + ";")
          | [](const auto &result) { return result.size() == 1; };
    }

    nonstd::optional<std::vector<std::string>>
    PostgresWsvQuery::getAccountRoles(const std::string &account_id) {
      return execute_(
                 "SELECT role_id FROM account_has_roles WHERE account_id = "
                 + transaction_.quote(account_id) + ";")
          | [&](const auto &result) {
              return transform<std::string>(result, [](const auto &row) {
                return row.at(kRoleId).c_str();
              });
            };
    }

    nonstd::optional<std::vector<std::string>>
    PostgresWsvQuery::getRolePermissions(const std::string &role_name) {
      return execute_(
                 "SELECT permission_id FROM role_has_permissions WHERE role_id "
                 "= "
                 + transaction_.quote(role_name) + ";")
          | [&](const auto &result) {
              return transform<std::string>(result, [](const auto &row) {
                return row.at("permission_id").c_str();
              });
            };
    }

    nonstd::optional<std::vector<std::string>> PostgresWsvQuery::getRoles() {
      return execute_("SELECT role_id FROM role;") | [&](const auto &result) {
        return transform<std::string>(
            result, [](const auto &row) { return row.at(kRoleId).c_str(); });
      };
    }

    nonstd::optional<std::shared_ptr<Account>> PostgresWsvQuery::getAccount(
        const std::string &account_id) {
      return execute_("SELECT * FROM account WHERE account_id = "
                      + transaction_.quote(account_id) + ";")
                 | [&](const auto &result)
                 -> nonstd::optional<std::shared_ptr<Account>> {
        if (result.empty()) {
          log_->info(kAccountNotFound, account_id);
          return nonstd::nullopt;
        }

        return makeAccount(result.at(0))
            .match(
                [](expected::Value<
                    std::shared_ptr<shared_model::interface::Account>> &v) {
                  return nonstd::make_optional(v.value);
                },
                [](expected::Error<std::shared_ptr<std::string>> &e)
                    -> nonstd::optional<
                        std::shared_ptr<shared_model::interface::Account>> {
                  return nonstd::nullopt;
                });
      };
    }

    nonstd::optional<std::string> PostgresWsvQuery::getAccountDetail(
        const std::string &account_id,
        const std::string &creator_account_id,
        const std::string &detail) {
      return execute_("SELECT data#>>"
                      + transaction_.quote("{" + creator_account_id + ", "
                                           + detail + "}")
                      + " FROM account WHERE account_id = "
                      + transaction_.quote(account_id) + ";")
                 | [&](const auto &result) -> nonstd::optional<std::string> {
        if (result.empty()) {
          log_->info(kAccountNotFound, account_id);
          return nonstd::nullopt;
        }
        auto row = result.at(0);
        std::string res;
        row.at(0) >> res;

        // if res is empty, then that key does not exist for this account
        if (res.empty()) {
          return nonstd::nullopt;
        }
        return res;
      };
    }

    nonstd::optional<std::vector<pubkey_t>> PostgresWsvQuery::getSignatories(
        const std::string &account_id) {
      return execute_(
                 "SELECT public_key FROM account_has_signatory WHERE "
                 "account_id = "
                 + transaction_.quote(account_id) + ";")
          |
          [&](const auto &result) {
            return transform<pubkey_t>(result, [&](const auto &row) {
              pqxx::binarystring public_key_str(row.at(kPublicKey));
              pubkey_t pubkey;
              std::copy(
                  public_key_str.begin(), public_key_str.end(), pubkey.begin());
              return pubkey;
            });
          };
    }

    nonstd::optional<std::shared_ptr<Asset>> PostgresWsvQuery::getAsset(
        const std::string &asset_id) {
      pqxx::result result;
      return execute_("SELECT * FROM asset WHERE asset_id = "
                      + transaction_.quote(asset_id) + ";")
                 | [&](const auto &result)
                 -> nonstd::optional<
                     std::shared_ptr<shared_model::interface::Asset>> {
        if (result.empty()) {
          log_->info("Asset {} not found", asset_id);
          return nonstd::nullopt;
        }
        return makeAsset(result.at(0))
            .match(
                [](expected::Value<
                    std::shared_ptr<shared_model::interface::Asset>> &v) {
                  return nonstd::make_optional(v.value);
                },
                [](expected::Error<std::shared_ptr<std::string>> &e)
                    -> nonstd::optional<
                        std::shared_ptr<shared_model::interface::Asset>> {
                  return nonstd::nullopt;
                });
      };
    }

    nonstd::optional<std::shared_ptr<AccountAsset>>
    PostgresWsvQuery::getAccountAsset(const std::string &account_id,
                                      const std::string &asset_id) {
      return execute_("SELECT * FROM account_has_asset WHERE account_id = "
                      + transaction_.quote(account_id)
                      + " AND asset_id = " + transaction_.quote(asset_id) + ";")
                 | [&](const auto &result)
                 -> nonstd::optional<
                     std::shared_ptr<shared_model::interface::AccountAsset>> {
        if (result.empty()) {
          log_->info("Account {} does not have asset {}", account_id, asset_id);
          return nonstd::nullopt;
        }

        return makeAccountAsset(result.at(0))
            .match([](expected::Value<
                       std::shared_ptr<shared_model::interface::AccountAsset>>
                          &v) { return nonstd::make_optional(v.value); },
                   [&](expected::Error<std::shared_ptr<std::string>> &e)
                       -> nonstd::optional<std::shared_ptr<
                           shared_model::interface::AccountAsset>> {
                     return nonstd::nullopt;
                   });
      };
    }

    nonstd::optional<std::shared_ptr<shared_model::interface::Domain>>
    PostgresWsvQuery::getDomain(const std::string &domain_id) {
      return execute_("SELECT * FROM domain WHERE domain_id = "
                      + transaction_.quote(domain_id) + ";")
                 | [&](const auto &result) -> nonstd::optional<std::shared_ptr<shared_model::interface::Domain>> {
        if (result.empty()) {
          log_->info("Domain {} not found", domain_id);
          return nonstd::nullopt;
        }
        return makeDomain(result.at(0))
            .match(
                [](expected::Value<
                    std::shared_ptr<shared_model::interface::Domain>> &v) {
                  return nonstd::make_optional(v.value);
                },
                [&](expected::Error<std::shared_ptr<std::string>> &e)
                    -> nonstd::optional<
                        std::shared_ptr<shared_model::interface::Domain>> {
                  return nonstd::nullopt;
                });
        ;
      };
    }

    nonstd::optional<
        std::vector<std::shared_ptr<shared_model::interface::Peer>>>
    PostgresWsvQuery::getPeers() {
      pqxx::result result;
      return execute_("SELECT * FROM peer;") | [&](const auto &result)
                 -> nonstd::optional<std::vector<
                     std::shared_ptr<shared_model::interface::Peer>>> {
        auto results = transform<BuilderResult<Peer>>(
            result, [](const auto &row) { return makePeer(row); });
        std::vector<std::shared_ptr<Peer>> peers;
        for (auto &r : results) {
          r.match(
              [&](expected::Value<std::shared_ptr<Peer>> &v) {
                peers.push_back(v.value);
              },
              [&](expected::Error<std::shared_ptr<std::string>> &e) {});
        }
        return peers;
      };
    }
  }  // namespace ametsuchi
}  // namespace iroha
