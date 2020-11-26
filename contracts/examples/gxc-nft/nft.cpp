#include <graphenelib/asset.h>
#include <graphenelib/contract.hpp>
#include <graphenelib/contract_asset.hpp>
#include <graphenelib/global.h>
#include <graphenelib/multi_index.hpp>
#include <graphenelib/system.h>
#include <graphenelib/dispatcher.hpp>
#include <map>
#include <set>
#include <tuple>
#include <math.h>

using namespace graphene;

static const int64_t BLACKHOLEACCOUNT = 3;
static const int64_t ADMINACCOUNT = 22;

class nft : public contract{
    public:
        nft(uint64_t account_id)
           : contract(account_id)
           , tokens(_self, _self)
           , accounts(_self, _self)
        {
        }

        //@abi action
        void mint(uint64_t to, uint64_t tokenid, std::string tokenuri){
            auto sender = get_trx_sender();
            graphene_assert(sender == ADMINACCOUNT , "You do not have permission");
            graphene_assert(_exists(tokenid) == false , "The tokenid is existed");
            tokens.emplace(sender,[&](auto &t){
                t.tokenid = tokenid;
                t.tokenuri = tokenuri;
                t.owner = to;
                t.approve = BLACKHOLEACCOUNT;
            });
            auto account_itr = accounts.find(to);
            if(account_itr == accounts.end()){
                accounts.emplace(sender,[&](auto &a){
                    a.owner = to;
                    a.tokenids.insert(tokenid);
                });
            } else {
                accounts.modify(*account_itr, sender, [&](auto &a){
                    a.tokenids.insert(tokenid);
                });
            }
        }

        //@abi action
        void approve(uint64_t tokenid, uint64_t to){
            auto sender = get_trx_sender();
            graphene_assert(_exists(tokenid) == true , "The tokenid is not existed");
            auto token_itr = tokens.find(tokenid);
            auto owner = token_itr->owner;
            graphene_assert(owner == sender || _isallapprove(owner, sender) , "You do not have permission");
            tokens.modify(*token_itr,sender,[&](auto &t){
                t.approve = to;
            });
        }

        //@abi action
        void transfer(uint64_t tokenid, uint64_t to){
            auto sender = get_trx_sender();
            graphene_assert(_exists(tokenid) == true , "The tokenid is not existed");
            auto token_itr = tokens.find(tokenid);
            uint64_t owner = token_itr->owner;
            graphene_assert(owner == sender || _isallapprove(owner, sender) || _getapproved(tokenid) == sender , "You do not have permission");
            tokens.modify(*token_itr, sender, [&](auto &t){
                t.owner = to;
                t.approve = BLACKHOLEACCOUNT;
            });
            auto old_itr = accounts.find(owner);
            auto new_itr = accounts.find(to);
            accounts.modify(*old_itr, sender, [&](auto &a){
                a.tokenids.erase(tokenid);
            });
            if(new_itr == accounts.end()){
                accounts.emplace(sender, [&](auto &a){
                    a.owner = to;
                    a.tokenids.insert(tokenid);
                });
            } else {
                accounts.modify(*new_itr, sender, [&](auto &a){
                a.tokenids.insert(tokenid);
                });
            }
        }

        //@abi action
        void burn(uint64_t tokenid){
            auto sender = get_trx_sender();
            graphene_assert(_exists(tokenid) == true , "The tokenid is not existed");
            auto token_itr = tokens.find(tokenid);
            uint64_t owner = token_itr->owner;
            graphene_assert(owner == sender, "You do not have permission");
            tokens.modify(*token_itr, sender, [&](auto &t){
                t.owner = BLACKHOLEACCOUNT;
                t.approve = BLACKHOLEACCOUNT;
            });
            auto old_itr = accounts.find(owner);
            accounts.modify(*old_itr, sender, [&](auto &a){
                a.tokenids.erase(tokenid);
            });
        }

        //@abi action 
        void approveall(uint64_t to){
            auto sender = get_trx_sender();
            graphene_assert( _isallapprove(sender, to) == false , "The account has been approved");
            auto owner_itr = accounts.find(sender);
            accounts.modify(*owner_itr, sender, [&](auto &a){
                a.allowance.insert(to);
            });
        }

        //@abi action
        void appallremove(uint64_t to){
            auto sender = get_trx_sender();
            graphene_assert( _isallapprove(sender, to) == true , "The account has not been approved");
            auto owner_itr = accounts.find(sender);
            accounts.modify(*owner_itr, sender, [&](auto &a){
                a.allowance.erase(to);
            });
        }

    private:
        uint64_t _getapproved(uint64_t tokenid){
            graphene_assert(_exists(tokenid) == true , "The tokenid is not existed");
            auto token_itr = tokens.find(tokenid);
            uint64_t approveid = token_itr->approve;
            return approveid;
        }

        bool _isallapprove(uint64_t owner, uint64_t _operator){
            auto account_itr = accounts.find(owner);
            graphene_assert( account_itr != accounts.end() , "The account does not have token");
            auto approve_itr = account_itr->allowance.find(_operator);
            return (approve_itr == account_itr->allowance.end() ? false : true);
        }

        bool _exists(uint64_t tokenid){
            auto token_itr = tokens.find(tokenid);
            return (token_itr == tokens.end() ? false : true);
        }

        //@abi table token i64
        struct token
        {
            uint64_t tokenid;
            std::string tokenuri;
            uint64_t owner;
            uint64_t approve;
            uint64_t primary_key() const { return tokenid; }
            GRAPHENE_SERIALIZE(token,(tokenid)(tokenuri)(owner)(approve))
        };
        typedef graphene::multi_index<N(token),token> token_index;
        token_index tokens;

        //@abi table account i64
        struct account
        {
           uint64_t owner;
           std::set<uint64_t> tokenids;
           std::set<uint64_t> allowance;
           uint64_t primary_key() const { return owner; }
           GRAPHENE_SERIALIZE(account,(owner)(tokenids)(allowance))
        };
        typedef graphene::multi_index<N(account),account> account_index;
        account_index accounts;
};
GRAPHENE_ABI(nft, (mint)(approve)(transfer)(burn)(approveall)(appallremove))