#ifndef EVALUATOR
#define EVALUATOR
#include <cstdint> // for std::uint32_t
#include "Card.h"

// copied from Decue's Table.py, which is a python implementation of the Cactus Kev's poker hand evaluator
// Yes, I am reinventing the wheel, yet again
struct Evaluator {

    struct LookUpTable{
        // magic numbers from Cactus Kev's poker hand evaluator
        constexpr static int MAX_STRAIGHT_FLUSH  = 10;
        constexpr static int MAX_FOUR_OF_A_KIND  = 166;
        constexpr static int MAX_FULL_HOUSE      = 322;
        constexpr static int MAX_FLUSH           = 1599;
        constexpr static int MAX_STRAIGHT        = 1609;
        constexpr static int MAX_THREE_OF_A_KIND = 2467;
        constexpr static int MAX_TWO_PAIR        = 3325;
        constexpr static int MAX_ONE_PAIR        = 6185;
        constexpr static int MAX_HIGH_CARD       = 7462;

        enum MAX_TO_RANK_CLASS {
            STRAIGHT_FLUSH = 1,
            FOUR_OF_A_KIND = 2,
            FULL_HOUSE = 3,
            FLUSH = 4,
            STRAIGHT = 5,
            THREE_OF_A_KIND = 6,
            TWO_PAIR = 7,
            ONE_PAIR = 8,
            HIGH_CARD = 9
        };

        constexpr static int straight_flushes[10]{
            7936,   // int('0b1111100000000', 2), # T J Q K A royal flush
            3968,   // int('0b0111110000000', 2), # 9 T J Q K
            1984,   // int('0b0011111000000', 2), # 8 9 T J Q
            992,    // int('0b0001111100000', 2), # 7 8 9 T J
            496,    // int('0b0000111110000', 2), # 6 7 8 9 T
            248,    // int('0b0000011111000', 2), # 5 6 7 8 9
            124,    // int('0b0000001111100', 2), # 4 5 6 7 8
            62,     // int('0b0000000111110', 2), # 3 4 5 6 7
            31,     // int('0b0000000011111', 2), # 2 3 4 5 6
            4111    // int('0b1000000001111', 2)  # A 2 3 4 5 small straight
        };

        static bool is_initialized;

        /*
        // thanks Gemini for reminding me on off-by-1 OOB error, which in fact would happen cuz the max values are well-defined as The Nut Straight Flush and The Nut Quad
        static int flush_lookup[31367010]; // 23*29*31*37*41 = 31367009, so flush_lookup[primt_product] stores the rank of the flush among all flushes
        static int unsuited_lookup[104553158]; //41*41*41*41*37 = 104553157, so unsuited_lookup[primt_product] stores the rank of the hand among all hands that are not flushes
        */
        // nvm, thanks Gemini again for reminding me that Deuces' implementation is not optimal.
        // directly using bitrank for flushes will be more efficient cuz shorter array that can be sucked into L1 cache and less function overhead
        static int flush_lookup[7937]; // AKQJT plus off-by-1, or 4096+2048+1024+512+256+1

        // The grown up naughty kid isn't that stupid. despite my freedom to code, why I ultimately follow Gemini's code? 
        // Humanity is like a flock of sheep
        // Nevertheless, thanks COMP2012 for actually being useful because I understood hashmap and probing without spending extra tokens to ask Gemini to explain
        struct FixedSizeHash16384 {
            struct HashEntry {int prime_product=0; int rank=0;};
            
            // Gemini said pick power of 2 as size to utilize bitshift and avoid modulus, sounds reasonable to me
            // and `1<<14` is a good balance between load factor `37.7%` and memory usage `16384*(4Bytes + 4Bytes) = 128KB` to be held by L2 Cache, also sounds reasonable to me
            HashEntry unsuited_entries[16384];
            
            // return the reference to rank given the prime product
            int& operator[](int prime_product) {
                // fibonacci hashing
                int hashed_prime_product = ((std::uint32_t)prime_product * 2654435769u)>>18;

                // linear probing. Taking the occassional flush pipeline penalty and keep L2 cache is better than slowly reading from RAM
                // it is also better than Binary Search because while that guy fits into L1 cache,
                while (unsuited_entries[hashed_prime_product].rank!=0 && unsuited_entries[hashed_prime_product].prime_product!=prime_product) {
                    hashed_prime_product = (hashed_prime_product+1) & 0x3FFF; // wrap around
                }

                // insert the prime_product if it is not already present
                if (unsuited_entries[hashed_prime_product].rank==0) unsuited_entries[hashed_prime_product].prime_product = prime_product;

                return unsuited_entries[hashed_prime_product].rank;
            }
        };

        static FixedSizeHash16384 unsuited_lookup;

        // the actual manual constructor that populates the lookup tables
        static void init() {
            if (is_initialized) return;

            LookUpTable::flushes();
            LookUpTable::multiples();
            is_initialized = true;
        } 

        static void flushes() {
            /**
             * This function populates the flush lookup table.
             * It calculates all possible flush combinations and their corresponding ranks.
             * straight flushes and flushes. 
             * Lookup is done on 13 bit integer (2^13 > 7462):
             * xxxbbbbb bbbbbbbb => integer hand index
             * 
             * Note that this also handles normal straights and high cards,
             * since they share the same prime products as straight flushes and flushes respectively.
             */
            int flushes[1287]; // 1277 + len(str_flushes) = 1287

            // 31 means 0b0000000011111 = 2,3,4,5,6
            int f = 0b0000000011111;

            // 1277 = number of high cards
            // 1277 + len(str_flushes) is number of hands with all cards unique rank
            // start from reverse order because SF leans left and flushes lean right by construction
            // perhaps two-pointer is a good idea
            int sf_idx = 0;
            constexpr static int sf_order[10]{8,7,6,5,4,3,2,1,9,0};
            int normal_flush_idx = 1286;
            // Note that straight flushes should be put inside [:10] and flushes should be put inside [10:1287]
            for (int i=1286;i>=0;--i) {
                //When i=0, f=0b1111100000000 the last flush AKQJT, which means sf_idx=9 and loop will be exited right before OOB

                // if the flush is actually a straight flush, it will trigger the special mechanic
                if (f==straight_flushes[sf_order[sf_idx]]) {
                    flushes[sf_order[sf_idx++]] = f; // write the straight flush into the front of the array
                } // if the flush is just normal flush, it will be written into the back of the array
                else {
                    flushes[normal_flush_idx--] = f; // write the flush into the back of the array
                }

                f = LookUpTable::get_lexographically_next_bit_sequence(f);
            }

            /**
             * now add to the lookup map:
             * start with straight flushes and the rank of 1
             * since theyit is the best hand in poker
             * rank 1 = Royal Flush!
            *
            int rank = 1;
            int prime_product;
            for (int sf : straight_flushes) {
                prime_product = Card::prime_product_from_rankbits(sf);
                this->flush_lookup[prime_product] = rank;
            }
            
            /**
             * we start the counting for flushes right after worst(i.e. max) full house, which is the worst rank that a full house can have (2,2,2,3,3)
             *
            rank = MAX_FULL_HOUSE + 1;
            for (int f : flushes) {
                int prime_product = Card::prime_product_from_rankbits(f);
                this->flush_lookup[prime_product] = rank;
                rank += 1;
            }

            /**
             * we can reuse these bit sequences for straights and high cards 
             * since they are inherently related and differ only by context
            *
            this->straight_and_highcards(flushes);*/
            
            /**
             * Actually, due to sf and straights sharing the same prime produces, as well as flushes and high cards sharing same prime produces
             * ima handle both at the same time
             */
            for (int rank=1; rank<=MAX_STRAIGHT_FLUSH; ++rank) {
                int prime_product = Card::prime_product_from_rankbits(flushes[rank-1]);
                LookUpTable::flush_lookup[flushes[rank-1]] = rank;
                LookUpTable::unsuited_lookup[prime_product] = rank + MAX_FLUSH; // straights are worse than flushes, so add MAX_FLUSH to offset the rank
            }
            /**
             * Sanity check 1: consider flushed 9TJQK, rankbits representation lives in flushes[1] with value of 0b0111110000000
             * At 2nd iteration, rank=2, Card::prime_product_from_rankbits(flushes[rank-1]) correctly referred to flushes[1]
             * And assume prime_product_from_rankbit(rankbits) works correctly, prime_product = 19*23*29*31*37 = 14535931
             * flush_lookup[14535931] = 2, unsuited_lookup[14535931] = 1601, which is correct since 1601 is the rank of 9TJQK among all possible hands
             * 
             * Sanity check 2: consider flushed A2345, rankbit representation lives in flushes[9] with value of 0b1000000001111
             * At 10th iteration, rank=10, Card::prime_product_from_rankbits(flushes[rank-1]) correctly referred to flushes[9]
             * And prime_prduct = 2*3*5*7*41 = 8610
             * flush_lookup[8610] = 10, unsuited_lookup[8610] = 1609, which is correct since 1609 is the rank of wheel A2345 among all possible hands
             */

            for (int rank=MAX_FULL_HOUSE+1; rank<=MAX_FLUSH; ++rank) {
                int prime_product = Card::prime_product_from_rankbits(flushes[rank-1+MAX_STRAIGHT_FLUSH-MAX_FULL_HOUSE]);
                LookUpTable::flush_lookup[flushes[rank-1+MAX_STRAIGHT_FLUSH-MAX_FULL_HOUSE]] = rank;
                LookUpTable::unsuited_lookup[prime_product] = rank - MAX_FULL_HOUSE + MAX_ONE_PAIR; // high cards are worse than one pair, so add ONE_PAIR to offset the rank
            }
            /**
             * Sanity check 1: consider flush AKQJ9 the highest non-straight flush, rankbit representation lives in flushes[10] with the value of 0b1111010000000
             * At 1st iteration, rank=323, Card::prime_product_from_rankbits(flushes[rank-1+MAX_STRAIGHT_FLUSH-MAX_FULL_HOUSE]) correctly referred to flushes[10]
             * And price_product = 19*29*31*37*41 = 25911877
             * flush_lookup[25911877] = 323 = MAX_FULL_HOUSE+1, unsuited_lookup[25911877] = 323 - 322 + 6185 = 6186 = MAX_ONE_PAIR+1
             * 
             * Sanity check 2: consider flush 23457 the lowest non-straight flush, rankbit representation lives in flushes[1286] with the value of 0b0000000101111
             * At 1277th iteration, rank=1599, Card::prime_product_from_rankbits(flushes[rank-1+MAX_STRAIGHT_FLUSH-MAX_FULL_HOUSE]) correctly referred to flushes[1286]
             * And price_product = 2*3*5*7*13 = 2184
             * flush_lookup[2184] = 1599 = MAX_FLUSH, unsuited_lookup[2184] = 1599 - 322 + 6185 = 7462 = MAX_HIGH_CARD, which is correct since 23457 is ofcourse the worst possible hand
             */
        }

        /*
        void straight_and_highcards(const int (&flushes)[1287]) {
            // Unique five card sets. Straights and highcards. 
            // Reuses bit sequences from flush calculations.

            int rank = MAX_FLUSH + 1; // straight's rank
            // fllushes[:10] are the straight flushes
            for (int straight=0; straight<10; ++straight) {
                int prime_product = Card::prime_product_from_rankbits(straight);
                this->unsuited_lookup[prime_product] = rank;
                rank += 1;
            }

            rank = MAX_ONE_PAIR + 1; // high card's rank
            // flushes[10:] are the normal flushes
            for (int highcard=10; highcard<1287; ++highcard) {
                int prime_product = Card::prime_product_from_rankbits(highcard);
                this->unsuited_lookup[prime_product] = rank;
                rank += 1;
            }
        }
        */

        static void multiples() {
            /**
             * Four of a kind, full house, three of a kind, two pair, pair.
             * Lookup is done on 32 bit integer (2^32 > 6185):
             * xxrrrxxx xxrrrxxx xxrrrxxx xxxrrrxx => integer hand index
             * where r is the rank of the card and x is the number of cards of that rank
            */
            constexpr static int backward_ranks[13] = {12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

            /*
            // 1) four of a kind
            int rank = MAX_STRAIGHT_FLUSH + 1;
            int prime_product = 1;
            // for each choice of a set of four rank
            for (int quad=12; quad>=0; --quad) {
                // and for each possible kicker rank
                for (int kicker=12; kicker>=0; --kicker) {
                    // turns out branchful is less punishing than memory writing to large chunk of array, so I guess I have to keep branchful
                    if (quad == kicker) continue; // except 5-of-a-kind
                    prime_product = Card::PRIMES[quad]*Card::PRIMES[quad]*Card::PRIMES[quad]*Card::PRIMES[quad] * Card::PRIMES[kicker];
                    this->unsuited_lookup[prime_product] = rank++;
                }
            }

            // 2) full house
            rank = MAX_FOUR_OF_A_KIND + 1;
            // for each choice of a set of three rank
            for (int trip=12; trip>=0; --trip) {
                // and for each possible pair rank
                for (int pair=12; pair>=0; --pair) {
                    if (trip == pair) continue; // except 5-of-a-kind
                    
                }
            }
            */
            // I think quad and FH can be handled in the same loop
            int major_prime_product, minor_prime_product = 0;
            int rank=1;
            for (int major=12; major>=0; --major) {
                for (int minor=12; minor>=0; --minor) {
                    if (major == minor) continue; // except 5-of-a-kind
                    // quad
                    major_prime_product = Card::PRIMES[major]*Card::PRIMES[major]*Card::PRIMES[major]*Card::PRIMES[major] * Card::PRIMES[minor];
                    // Full House
                    minor_prime_product = Card::PRIMES[major]*Card::PRIMES[major]*Card::PRIMES[major] * Card::PRIMES[minor]*Card::PRIMES[minor];
                    LookUpTable::unsuited_lookup[major_prime_product] = MAX_STRAIGHT_FLUSH + rank; // quad
                    LookUpTable::unsuited_lookup[minor_prime_product] = MAX_FOUR_OF_A_KIND + rank++; // full house
                }
            }


            // 3) three of a kind
            rank = MAX_STRAIGHT + 1;
            // for each choice of a set of three rank
            for (int trip=12; trip>=0; --trip) {
                // for each larger kicker
                for (int kicker1=12; kicker1>=0; --kicker1) {
                    // for each smaller kicker
                    for (int kicker2=kicker1-1; kicker2>=0; --kicker2) {
                        if (trip == kicker1 || trip == kicker2) continue; // except 4-of-a-kind, could be FH cuz kicker1 > kicker2
                        int prime_product = Card::PRIMES[trip]*Card::PRIMES[trip]*Card::PRIMES[trip] * Card::PRIMES[kicker1] * Card::PRIMES[kicker2];
                        LookUpTable::unsuited_lookup[prime_product] = rank++;
                    }
                }
            }

            // 4) two pair
            // rank = MAX_THREE_OF_A_KIND + 1 at this point, no need to re-define
            for (int pair1 = 12; pair1 >= 0; --pair1) {
                for (int pair2 = pair1 - 1; pair2 >= 0; --pair2) {
                    for (int kicker=12; kicker >= 0; --kicker) {
                        if (kicker == pair1 || kicker == pair2) continue; // except full house
                        int prime_product = Card::PRIMES[pair1]*Card::PRIMES[pair1] * Card::PRIMES[pair2]*Card::PRIMES[pair2] * Card::PRIMES[kicker];
                        LookUpTable::unsuited_lookup[prime_product] = rank++;
                    }
                }
            }

            // I think trips and 2pairs can also be handled in the same loop
            /* nvm, because they use totally different logic that im too lazy to think about the exception scenario
            rank = 1;
            for (int major=12; major>=0; --major) {
                for (int minor=12; minor>=0; --minor) {
                    for (int kicker=12; kicker>=0; --kicker) {
                        if (major == minor || major == kicker || minor == kicker) continue; // except 4-of-a-kind and full house or other weird duplications
                        // trips
                        major_prime_product = Card::PRIMES[major]*Card::PRIMES[major]*Card::PRIMES[major] * Card::PRIMES[minor] * Card::PRIMES[kicker];
                        // 2pairs
                        minor_prime_product = Card::PRIMES[major]*Card::PRIMES[major] * Card::PRIMES[minor]*Card::PRIMES[minor] * Card::PRIMES[kicker];
                        this->unsuited_lookup[major_prime_product] = MAX_STRAIGHT + rank; // trips
                        this->unsuited_lookup[minor_prime_product] = MAX_THREE_OF_A_KIND + rank++; // 2pairs
                    }
                }
            }*/

            // 5) one pair
            // rank = MAX_TWO_PAIR + 1;
            for (int pair=12; pair>=0; --pair) {
                for (int kicker1=12; kicker1>=0; --kicker1) {
                    for (int kicker2=kicker1-1; kicker2>=0; --kicker2) {
                        for (int kicker3=kicker2-1; kicker3>=0; --kicker3) {
                            if (pair == kicker1 || pair == kicker2 || pair == kicker3) continue; // except trips
                            int prime_product = Card::PRIMES[pair]*Card::PRIMES[pair] * Card::PRIMES[kicker1] * Card::PRIMES[kicker2] * Card::PRIMES[kicker3];
                            LookUpTable::unsuited_lookup[prime_product] = rank++;
                        }
                    }
                }
            }

        }

        static int get_lexographically_next_bit_sequence(int v) {
        /**
         * Bit hack from here (wow the original is in C so no need to convert from python!):
         * http://www-graphics.stanford.edu/~seander/bithacks.html#NextBitPermutation
         * 
         * Generator even does this in poker order rank 
         * so no need to sort when done! Perfect.
         * 
         * @param v: current permutation of bits
         * @return: next permutation of bits with same number of bits set
         */ 
        
            int t = (v | (v - 1)) + 1;
            return t | ((((t & -t) / (v & -v)) >> 1) - 1);
        }
    };

    LookUpTable table;

    int evaluate_once(const int (&cards)[7]) {
        /**
         * Evaluates a 7 card hand once.
         * 
         * @param cards: array of 7 integers representing the cards in the hand
         * @return: integer rank of the hand (1 = best, 7462 = worst)
        */
        constexpr static int EVALUATION_ORDER[21][5] = {
            {0, 1, 2, 3, 4},
            {0, 1, 2, 3, 5},
            {0, 1, 2, 3, 6},
            {0, 1, 2, 4, 5},
            {0, 1, 2, 4, 6},
            {0, 1, 2, 5, 6},
            {0, 1, 3, 4, 5},
            {0, 1, 3, 4, 6},
            {0, 1, 3, 5, 6},
            {0, 1, 4, 5, 6},
            {0, 2, 3, 4, 5},
            {0, 2, 3, 4, 6},
            {0, 2, 3, 5, 6},
            {0, 2, 4, 5, 6},
            {0, 3, 4, 5 ,6},
            {1 ,2 ,3 ,4 ,5},
            {1 ,2 ,3 ,4 ,6},
            {1 ,2 ,3 ,5 ,6},
            {1 ,2 ,4 ,5 ,6},
            {1 ,3 ,4 ,5 ,6},
            {2 ,3 ,4 ,5 ,6}
        };

        int handOR, prime_product;
        int current_rank = 7463; // worst rank possible + 1
        int best_rank = 7462; // worst rank possible
        for (const int (&evaluate)[5]: EVALUATION_ORDER) {
            const int card1 = cards[evaluate[0]];
            const int card2 = cards[evaluate[1]];
            const int card3 = cards[evaluate[2]];
            const int card4 = cards[evaluate[3]];
            const int card5 = cards[evaluate[4]];

            /* IMPLEMENTATION FROM DEUCES LIBRARY:
            def _five(self, cards):
            """
            Performs an evalution given cards in integer form, mapping them to
            a rank in the range [1, 7462], with lower ranks being more powerful.

            Variant of Cactus Kev's 5 card evaluator, though I saved a lot of memory
            space using a hash table and condensing some of the calculations. 
            """
            # if flush
            if cards[0] & cards[1] & cards[2] & cards[3] & cards[4] & 0xF000:
                handOR = (cards[0] | cards[1] | cards[2] | cards[3] | cards[4]) >> 16
                prime = Card.prime_product_from_rankbits(handOR)
                return self.table.flush_lookup[prime]

            # otherwise
            else:
                prime = Card.prime_product_from_hand(cards)
                return self.table.unsuited_lookup[prime]
            */
            if (card1 & card2 & card3 & card4 & card5  &0xF000) {
                handOR = (card1 | card2 | card3 | card4 | card5) >> 16;
                current_rank = this->table.flush_lookup[handOR];
                best_rank = current_rank < best_rank ? current_rank : best_rank;
            }
            else {
                prime_product = Card::prime_product_from_hand(card1, card2, card3, card4, card5);
                current_rank = this->table.unsuited_lookup[prime_product];
                best_rank =  current_rank < best_rank ? current_rank : best_rank;
            }
        }
        return best_rank;
    }

    int evaluate_once(const int (&hole)[2], const int (&board)[5]) {
        /**
         * Evaluates a 7 card hand once. Used for populating the look-up table.
         * 
         * @param hole: array of 2 integers representing the hole cards
         * @param board: array of 5 integers representing the board cards
         * @return: integer rank of the hand (1 = best, 7462 = worst)
        */
        const int cards[7] = {hole[0], hole[1], board[0], board[1], board[2], board[3], board[4]};
        return evaluate_once(cards);
    }

    // apparently this overhead has to be paid anyway
    int evaluate_once(int card1, int card2, int card3, int card4, int card5, int card6, int card7) {
        const int cards[7] = {card1, card2, card3, card4, card5, card6, card7};
        return evaluate_once(cards);
    }
};

#endif