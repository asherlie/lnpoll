#include <lfh.h>
#include <localnotify.h>
#include <stdint.h>
#include <stdatomic.h>
/*
users can use localnotify to send eth packets to vote on polls

users cannot spoof the src addr field of packets, so it's verifiable where they come from
this is serverless so how do we collect and count?

maybe we start polling, expecting a certain amount of responses


./poll --start-poll <name> <option 0> <option 1> <...> // start a poll with a name and >= 2 options
./poll --vote-poll <name> <int option>
./poll --force-close-poll <name>
./poll --list-polls
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
    char poll_name[11];
    // TODO: remove n_opts if possible, or create a new struct
    // to use as key for lfh
    // this way the user won't need to provide n_opts from cli
    uint16_t n_opts;
};

// this struct is used to send votes
// polls are stored locally in lfhashes
struct poll_vote{
    struct poll_hdr p;
    uint16_t vote;
};

// local representation of a poll
struct results{
    uint16_t* _Atomic votes;
};

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

/* TODO: should maddr be a *? */
// why is it phdr: maddr: int?
// it should probably be maddr: list of poll_hdrs, 
/*
 * register_lockfree_hash(struct maddr*, uint16_t, poll_results)
 * register_lockfree_hash(struct poll_hdr*, poll_results, polls)
*/
// v is an atomic map of results for a given poll
// TODO: test ith poll_hdr*, seems intuitively like this won't work but make sure of that
register_lockfree_hash(struct poll_hdr, struct results, polls)

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
void vote(polls* p, struct maddr creator, uint8_t poll_id, char* poll_name, uint16_t n_opts, uint16_t vote){
    _Bool found;
    struct poll_hdr hdr = {.creator = creator, .poll_id = poll_id, .n_opts = n_opts};
    struct results res;
    memset(hdr.poll_name, 0, sizeof(hdr.poll_name));
    strcpy(hdr.poll_name, poll_name);
    res = lookup_polls(p, hdr, &found);

    if (!found || !res.votes) {
        return;
    }

    // increment locally? nope i should probably do this only from the lnotify thread
    // i think it'll receive a copy from sender as well!
    // vote thread will handle this
    //
    // vote() will just push a lnotify message
    atomic_fetch_add(&res.votes[vote], 1);
}

_Bool p_results(polls* p, struct maddr creator, uint8_t poll_id, char* poll_name, uint16_t n_opts){
    _Bool found;
    struct poll_hdr hdr = {.creator = creator, .poll_id = poll_id, .n_opts = n_opts};
    struct results res;
    memset(hdr.poll_name, 0, sizeof(hdr.poll_name));
    strcpy(hdr.poll_name, poll_name);
    res = lookup_polls(p, hdr, &found);

    if (!found) {
        return 0;
    }

    printf("POLL RESULTS FOR \"%s\":\n", poll_name);
    printf("  [%d", atomic_load(res.votes));
    for (int i = 1; i < n_opts; ++i) {
        printf(", %d", atomic_load(&res.votes[i]));
    }
    puts("]");

    return 1;
}

void test(polls* p, uint16_t n_opts){
    struct poll_hdr hdr = {0};
    struct results res = {.votes = calloc(n_opts, sizeof(uint16_t* _Atomic))};
    
    strcpy(hdr.poll_name, "asherpol");
    hdr.n_opts = n_opts;
    hdr.poll_id = 9;
    insert_polls(p, hdr, res);

    vote(p, hdr.creator, hdr.poll_id, hdr.poll_name, hdr.n_opts, 8);
    vote(p, hdr.creator, hdr.poll_id, hdr.poll_name, hdr.n_opts, 8);
    vote(p, hdr.creator, hdr.poll_id, hdr.poll_name, hdr.n_opts, 0);

    p_results(p, hdr.creator, hdr.poll_id, hdr.poll_name, hdr.n_opts);
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
    p_maddr(local_addr);
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
