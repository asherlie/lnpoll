#include <lfh.h>
#include <stdint.h>
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

/* TODO: should this not take a *? */
uint16_t hash(struct maddr* a){
    uint16_t ret = 0;
    for (int i = 0; i < 6; ++i){
        ret += a->addr[i];
    }
    return ret;
}

register_lockfree_hash(struct maddr*, uint16_t, poll_results)

struct poll{
    uint16_t n_opts;
    poll_results res;
};

void init_poll(struct poll* p, uint16_t n_opts){
    p->n_opts = n_opts;
    init_poll_results(&p->res, 10, hash);
}

int main(){
}
