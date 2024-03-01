#include <lfh.h>
#include <localnotify.h>
#include <stdint.h>
#include <stdatomic.h>
// NOTE: keys must be memset to 0 to zero the garbage bytes
// TODO: replace all local_addrs with something smarter
// TODO: unix sockets will be used to send commands to daemon and receive output
//       this will be trivial because there will be no keeping connections alive
//       sockets will have one send and one recieve in their lifetime
//       parsing will be done completely by the daemon - the raw string stdin will
//       be passed along to daemon 
//
//       wait, think twice before writing any unix socket code
//       i can probably handle everything using lnotify
//       nvm, this will broadcast to everyone so there must be a difference between that
//       and local messages on one machine
//
//       BUT
//       lnotify will fully handle the creation of new polls, but that will just
//       hve to be parsed from --start-poll and done all on the daemon
//
//       just make sure to have the lnotify thread create ALL polls, even ones
//       where i am the creator
//
//       unix sockets will handle passing of messages ONLY
/*
users can use localnotify to send eth packets to vote on polls

users cannot spoof the src addr field of packets, so it's verifiable where they come from
this is serverless so how do we collect and count?

maybe we start polling, expecting a certain amount of responses


./poll --start-poll <name> <option 0> <option 1> <...> // start a poll with a name and >= 2 options
./poll --vote-poll <name> <int option>
./poll --force-close-poll <name>
./poll --list-polls --unvoted
./poll --get-results <name>

each new poll needs either a time limit or a max number of responses

seems like poll is a program that's used to interact with a daemon
our daemon will collect poll requests and wall()/beep(), letting users know

should it take in min_responses?
once this is reached poll can be closed

maybe there will be an output file, this is where results are printed?


this is cool because it uses both locnotify and lfhash

register_lfhash(poll, addr[6], int)
*/

/*
 * a poll is an integer for number of options and 
 * 
 * struct poll{
 *     4 options;
 *     addr -> uint;
 * };
*/
struct maddr{
    uint8_t addr[6];
};

// a poll header can maybe just be the packet that notifies for all polls, either creation or voting
// if !lookup(poll_hdr), insert(poll_hdr)
// n_opts can be used as vote if we found a matching poll
// the only issue with this is that a vote can be interpreted as n_polls if a new machine comes online
// ah, i see, we only add a new poll if maddr creator == sender of packet
//  this is only a safeguard, however, if a user cannot vote on his own poll
//  nvm. this is all fixed by adding a creator_vote field. a creator cannot change his vote
//  nvm, he can.
//  oh, how can we use it as a key when we're altering some fields
//  may ned to split it into to two and only use the const fields as a key - creator, poll_id, n_opts
//
//  this packet is sent out, redefine toremove votes from it
//  if !lookup(phdr), insert()
//  else, vote
//
//  a poll is a unique combination of creator, id, n_opts
//
//  TODO: remove creator_vote
//  place elsewhere, also no need for CREATOR_vote, just add vote
//  all users can vote using same field
struct poll_hdr{
    struct maddr creator;
    uint8_t poll_id;
    /*char poll_name[31];*/
    // TODO: remove n_opts if possible, or create a new struct
    // to use as key for lfh
    // this way the user won't need to provide n_opts from cli
    uint16_t n_opts;
};

register_lockfree_hash(struct maddr, uint16_t, results)

struct poll_info{
    char poll_name[31];
    char memo[41];
};

// this contains everything needed to add a poll, poll_hdr contains all that is necessary
// to lookup polls, poll_info contains poll name and memo
// poll_info is parsed 
struct poll{
    /*struct poll_hdr hdr;*/
    struct poll_info info;
    results r;
};

/*register_lockfree_hash(struct poll_hdr, struct poll_info, polls)*/
register_lockfree_hash(struct poll_hdr, struct poll, polls)


// this struct is used to send votes
// polls are stored locally in lfhashes
// actual sent packets:

struct poll_pkt{
    struct poll_hdr hdr;
    struct poll_info info;
};

struct poll_vote_pkt{
    struct poll_hdr p;
    uint16_t vote;
};

    struct poll _lookup_polls(polls* l, struct poll_hdr key, _Bool* found) {
        struct poll ret;
        struct poll_hdr* kptr;
        memset(&ret, 0, sizeof(struct poll)); 
        *found = 0; 
        foreach_entry_kptrv(polls, l, key, kptr, ret) 
            if (!memcmp(kptr, &key, sizeof(struct poll_hdr))){ 
                *found = 1; 
                /* interesting! this is only failing with pointer keys! value can be anything, pointer KEYS are bad */ 
                return ret; 
            } 
        } 
        return ret; 
    } 

// local representation of a poll

/* TODO: should this not take a *? */
uint16_t hash_m(struct maddr* a){
    uint16_t ret = 0;
    for (int i = 0; i < 6; ++i){
        ret += a->addr[i];
    }
    return ret;
}

uint16_t hash_ph(struct poll_hdr ph){
    return hash_m(&ph.creator);
}

uint16_t hash_ma(struct maddr ma){
    return hash_m(&ma);
}

/* TODO: should maddr be a *? */
// why is it phdr: maddr: int?
// it should probably be maddr: list of poll_hdrs, 
/*
 * register_lockfree_hash(struct maddr*, uint16_t, poll_results)
 * register_lockfree_hash(struct poll_hdr*, poll_results, polls)
*/
// v is an atomic map of results for a given poll
// TODO: test ith poll_hdr*, seems intuitively like this won't work but make sure of that

/*
 * a poll needs to be:
 *     indexable by poll_hdr
 *     indexable by creator maddr + name
 * register_lockfree_hash();
*/

/*
 * struct poll{
 *     // TODO: look at ashnet to see how to get local eth
 *     uint8_t poll_id[7];
 *     uint16_t n_opts;
 *     poll_results res;
 * };
*/

/*
 * void init_poll(polls* p, uint16_t n_opts){
 *     [>init_poll_results(&p->res, 100, hash);<]
 *     init_polls(p, 100, hash_ph);
 * }
*/

// insert polls uses explicit struct poll_hdr that is received using lnotify

// the actual vote can be much simpler than this because it'll have access to explicit poll_hdr
void vote(polls* p, struct maddr creator, uint8_t poll_id, uint16_t n_opts, uint16_t vote){
    _Bool found;
    struct poll_hdr hdr = {0};
    /*struct poll_info pi;*/
    struct poll pl;
    struct maddr addr = {0};
    /*memset(hdr.poll_name, 0, sizeof(hdr.poll_name));*/
    /*strcpy(hdr.poll_name, poll_name);*/

    hdr.creator = creator;
    hdr.poll_id = poll_id;
    hdr.n_opts = n_opts;

    pl = lookup_polls(p, hdr, &found);

    /*if (!found || !res.votes) {*/
    if (!found || vote >= n_opts) {
        return;
    }

    // increment locally? nope i should probably do this only from the lnotify thread
    // i think it'll receive a copy from sender as well!
    // vote thread will handle this
    //
    // vote() will just push a lnotify message
    /*atomic_fetch_add(&res.votes[vote], 1);*/
    // TODO: this will be done implicitly by the voter thread, it'll grab the maddr field
    // TODO: receive payload must optionally take in a field to set sender mac address
    // this is what will be used to actually vote
    get_local_addr("wlp3s0", addr.addr);
    /*addr.addr[0] = rand();*/
    /*insert_results(&pi.r, addr, vote);*/
    // we never get here, stuck on lookup_polls()
    insert_results(&pl.r, addr, vote);
}

_Bool p_results(polls* p, struct maddr creator, uint8_t poll_id, uint16_t n_opts, _Bool indent){
    _Bool found;
    struct poll_hdr hdr = {0};
    struct poll pl;
    /*memset(hdr.poll_name, 0, sizeof(hdr.poll_name));*/
    /*strcpy(hdr.poll_name, poll_name);*/
    // this is apparently failing
    /*
     * struct poll_hdr* kp;
     * struct poll plcheck;
     * (void)kp; (void)plcheck;
    */

    hdr.creator = creator;
    hdr.poll_id = poll_id;
    hdr.n_opts = n_opts;
    // so weird, simply calling foreach before lookup() is solving the problem
    /*foreach_entry_kptrv(polls, p, hdr, kp, plcheck)*/
        /*
         * printf("found %d\n", kp->poll_id);
         * printf("found %s %s\n", plcheck.info.poll_name, plcheck.info.memo);
        */
    /*}*/
    pl = lookup_polls(p, hdr, &found);


    if (!found) {
        return 0;
    }

    /*printf("POLL RESULTS FOR \"%s\" : \"%s\":\n", pl.info.poll_name, pl.info.memo);*/

    /*ugh... i really should just have an iterate over ALL entries macro*/
    for (uint16_t i = 0; i < pl.r.n_buckets; ++i) {
        foreach_entry_idx(results, &pl.r, i, _ep)
            if (indent) {
                printf("\t");
            }
            p_maddr(_ep->kv.k.addr);
            /*
             * if (indent) {
             *     printf("\t");
             * }
            */
            printf(": %d\n", atomic_load(&_ep->kv.v));
        /*foreach_entry_kv(results, &pi.r, key, iterk, iterv)*/
        }
    }

    /*
     * printf("  [%d", atomic_load(res.votes));
     * for (int i = 1; i < n_opts; ++i) {
     *     printf(", %d", atomic_load(&res.votes[i]));
     * }
     * puts("]");
    */

    return 1;
}

uint16_t list_polls(polls* p, struct maddr* creator, uint8_t* poll_id, uint16_t* n_opts, _Bool unvoted_only, _Bool show_results){
    /*go through all polls, filter by any provided args, print summary or contents of each*/
    struct poll_hdr ph;
    struct poll v;
    struct maddr local_addr = {0};
    uint16_t ret = 0;
    _Bool found;
    for (uint16_t i = 0; i < p->n_buckets; ++i) {
        foreach_entry_idx(polls, p, i, _ep)
            ph = _ep->kv.k;
            if ((creator && memcmp(creator->addr, ph.creator.addr, 6)) || (poll_id && *poll_id != ph.poll_id) ||
                (n_opts && *n_opts != ph.n_opts)) {
                // i'm assuming that these `continue`s will continue foreach_entry_idx. test to make sure of this
                continue;
            }
            v = atomic_load(&_ep->kv.v);
            // TODO: get_local_addr() should only be called once
            get_local_addr("wlp3s0", local_addr.addr);
            lookup_results(&v.r, local_addr, &found);
            if (unvoted_only && found) {
                continue;
            }
            ++ret;
            printf("CREATOR: ");
            p_maddr(ph.creator.addr);
            printf(", ID: %d, OPTIONS: %d:\n", ph.poll_id, ph.n_opts);
            printf(" \"%s\" ; \"%s\"\n", v.info.poll_name, v.info.memo);

            if (show_results) {
                p_results(p, ph.creator, ph.poll_id, ph.n_opts, 1);
            }
        }
    }
    return ret;
}

struct poll create_spoof_p(char* poll_name, char* memo){
    struct poll p = {0};
    strcpy(p.info.poll_name, poll_name);
    strcpy(p.info.memo, memo);
    init_results(&p.r, 100, hash_ma);
    return p;
}

void test(polls* p, uint16_t n_opts){
    struct poll_hdr hdr = {0};
    /*struct results res = {.votes = calloc(n_opts, sizeof(uint16_t* _Atomic))};*/
    /*struct poll pl;*/
    /*struct maddr nil_addr = {0};*/
    
    hdr.creator.addr[0] = 1;
    /*
     * strcpy(pl.info.memo, "0: asher, 1: basher");
     * strcpy(pl.info.poll_name, "asher's first poll");
     * init_results(&pl.r, 1, hash_ma);
    */
    // ah, i see what should be done - the initiator should send out an optional descriptor
    // as part of poll_name - poll_memo, 
    // NVM! add a field for poll_memo!
    // this will be always inserted as NULL
    // it will be stored locally but will not be needed to lookup a poll
    // perhaps it should live in the results struct! this way we can access it during lookup only
    // we'll foreach() when using the --list-polls --unvoted flag
    

/*
 *     OOPS! the uint16_t votes won't work. users need to be held accountable. it must be another lfh
 *     addressable by maddr -> uint16_t
 * 
 *     my idea of memos stored locally can still work, just needs to go within a struct
 *                                         ~~~~ ~~~~ ~~~~ ~~~~
 *     that can be intermediary - polls[hdr] -> struct results: memo, lfh_maddr_to_uint
 *                                         ~~~~ ~~~~ ~~~~ ~~~~
 * 
*/
    /*strcpy(hdr.poll_name, "asherpol, please mark");*/
    hdr.n_opts = n_opts;
    hdr.poll_id = 9;
    // simulate creating a new poll
    insert_polls(p, hdr, create_spoof_p("poll 0", "guide 0"));
    ++hdr.creator.addr[0];
    insert_polls(p, hdr, create_spoof_p("poll 1", "guide 1"));
    ++hdr.creator.addr[0];
    insert_polls(p, hdr, create_spoof_p("poll 2", "guide 2"));
    ++hdr.creator.addr[0];
    insert_polls(p, hdr, create_spoof_p("NEW 0", "NEW guide 1"));
    ++hdr.creator.addr[0];
    hdr.poll_id = 99;
    insert_polls(p, hdr, create_spoof_p("new 1", "new guide 1"));
    /*hdr.creator.addr[0] = 1;*/

    /*vote(p, hdr.creator, hdr.poll_id, hdr.n_opts, 8);*/
    /*vote(p, hdr.creator, hdr.poll_id, hdr.n_opts, 8);*/
    /*vote(p, hdr.creator, hdr.poll_id, hdr.n_opts, 1);*/
    /*vote(p, hdr.creator, hdr.poll_id, hdr.n_opts, 0);*/
    vote(p, hdr.creator, hdr.poll_id, hdr.n_opts, 3);
    /*
     * vote(p, hdr.creator, hdr.poll_id, hdr.poll_name, hdr.n_opts, 0);
     * vote(p, hdr.creator, hdr.poll_id, hdr.poll_name, hdr.n_opts, 80);
    */

    /*p_results(p, hdr.creator, hdr.poll_id, hdr.n_opts);*/
    uint8_t poll_id = 99;
    list_polls(p, NULL, &poll_id, NULL, 1, 1);
}

int main(int argc, char* argv[]){
    uint8_t local_addr[6];
    char* if_name;
    polls p;

    if (argc < 2) {
        return 1;
    }

    if_name = argv[1];
    /*init_poll(&p, 4);*/
    get_local_addr(if_name, local_addr);
    /*p_maddr(local_addr);*/
    /*puts("");*/
    init_polls(&p, 100, hash_ph);

    test(&p, 10);
}

/*
 * hmm, if i'm just waiting for localnotifies in a thread then there's a chance of missing an alert while we're busy
 * discarding an irrelevant one or processing a releavant one
 * 
 * NVM, i believe recvfrom() just pops from a queue for a socket. the syscall will handle the backlog
 * 
 * i can just have a reader thread that gets notifies, receives an action struct, and calls take_action(action struct)
*/
