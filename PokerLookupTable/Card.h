#ifndef CARD
#define CARD
#include <iostream>
#include <utility> // for std::swap
#include <cstdint> // for std::uint64_t
// turns out reinventing the wheel is actually a good idea cuz I learnt a few more optimization techniques in C++ in the process
// And a few new algorithm that Gemini suggested me to use (and because this is god damn Gemini, I have to independantly verify everything it suggested >.<)
struct Card {
    /*
    Static class that handles cards. We represent cards as 32-bit integers, so 
    there is no object instantiation - they are just ints. Most of the bits are 
    used, and have a specific meaning. See below: 

                                    Card:

                            bitrank     suit rank   prime
                    +--------+--------+--------+--------+
                    |xxxbbbbb|bbbbbbbb|shcdrrrr|xxpppppp|
                    +--------+--------+--------+--------+

        1) p = prime number of rank (deuce=2,trey=3,four=5,...,ace=41)
        2) r = rank of card (deuce=0,trey=1,four=2,five=3,...,ace=12)
        3) shcd = suit of card (bit turned on based on suit of card)
        4) b = bit turned on depending on rank of card
        5) x = unused

    Note that the mapping of r here and r in the ML component is different, as here is ascending from 2 and there is descending from A
    */


                            /*--- BEG OF SETUP ---*/

    int cardbits; // I am most likely the sole developer of this code, so ima enable public access and ignore code safety
    constexpr static int PRIMES[13] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41};
    constexpr static int INT_RANKS[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}; // 2, 3, 4 ... Q, K, A
    constexpr Card(): cardbits(0) {};
    constexpr Card(int cardbits) : cardbits(cardbits) {}
    constexpr Card(int rank, int suit) : cardbits((1 << (rank + 16)) | ((1<<suit) << 12) | (rank << 8) | PRIMES[rank]) {}
    constexpr Card(const Card& other) = default; // COMP2012 has betrayed me. Turns out allowing copy by value is actually not not guaranteed to be slower, especially if the class is small. But on the bright side, at least I learnt a new exception to the the heuristics :D
    constexpr operator int() const {return cardbits;} // allow implicit conversion to int for convenience. Thank you Gemini for suggesting it

    // Thanks whoever deciding that member functions are automatically inlined when designing C++ compiler
    constexpr static int get_rank_int(int card) {return (card >> 8) & 0xF;}
    constexpr static int get_bitrank_int(int card) {return (card >> 16) & 0x1FFF;}
    constexpr static int get_prime(int card) {return card & 0xFF;}
    static int get_suit_int(int card) {return __builtin_ctz((card >> 12) & 0xF);} // COMP2012 successfully influenced my computer to have C++11 compiler the heritage
    
    constexpr int get_rank_int() const {return get_rank_int(cardbits);}
    constexpr int get_bitrank_int() const {return get_bitrank_int(cardbits);}
    constexpr int get_prime() const {return get_prime(cardbits);}
    int get_suit_int() const {return get_suit_int(cardbits);} // this guy is crying in a corner cuz he doesnt fit into the constexpr gang >.<
                            /*--- END OF SETUP ---*/

                    /*--- THE ACTUALLY USEFUL FCUNTIONS ---*/
    static int prime_product_from_hand(const int (&cards)[5]) { // modify: initial value of reference to non-const must be an lvalue
        /**
         * Returns the prime product of the 5 cards in the hand.
         * 
         * @param cards: array of 5 Card objects
         * @return: integer prime product of the 5 cards
        */
        int product = 1;
        for (int i=0;i<5;++i) {
            product *= get_prime(cards[i]);
        }
        return product;
    }

    // To avoid tmp object creation in Evaluator::evaluate_once(...)
    // deliberately pick pbv over pbr for obvious reason
    static int prime_product_from_hand(int c1, int c2, int c3, int c4, int c5) {
        /**
         * Returns the prime product of the 5 cards in the hand.
         * 
         * @param c1: first card
         * @param c2: second card
         * @param c3: third card
         * @param c4: fourth card
         * @param c5: fifth card
         * @return: integer prime product of the 5 cards
        */
        return get_prime(c1) * get_prime(c2) * get_prime(c3) * get_prime(c4) * get_prime(c5);
    }

    static int prime_product_from_rankbits(int rankbits) {
        /**
         * Returns the prime product using the bitrank (b)
         * bits of the hand. Each 1 in the sequence is converted
         * to the correct prime and multiplied in.
         * 
         * 
         * Primarily used for evaulating flushes and straights, 
         * two occasions where we know the ranks are *ALL* different.
         * 
         * Assumes that the input is in form (set bits):
                         rankbits     
                    +--------+--------+
                    |xxxbbbbb|bbbbbbbb|
                    +--------+--------+
        */
        int product = 1;
        while (rankbits) {
            int idx = __builtin_ctz(rankbits); // get the index of the least significant set bit
            product *= PRIMES[idx];
            rankbits &= rankbits - 1; // Thank you Brian Kernighan
        }
        return product;
    }

    /* UNUSED
    static int get_weight_index(int cardbit) {
        /**
         * Returns the index of the card in the weight array of Deck class.
         * 
         * @param cardbit: integer representation of the card
         * @return: integer index of the card in the weight array
        *
        return (get_rank_int(cardbit)*4) + get_suit_int(cardbit);
    }
    */

    static int get_13x13_index(int c1, int c2) {
        /**
         * Returns the index of the 2-card hand in a 13x13 matrix in Solver GUI. Note that the mapping is different from the basis here
         * AA should sit at top left with index 0, AKs at index 1, A2s at index 12, AKo at index 13, A2o at index 156, and 22 at index 168.
         * 
         * @param c1: first card
         * @param c2: second card
         * @return: integer index of the 2-card hand in the flattened 13x13 matrix
        */
        // sanitize the order of c1 and c2 so that c1 is always the higher rank card
        /*if (c1 == c2) { // I think it is ok to take the risk without exception to save runtime, lets see how well it goes
            throw std::invalid_argument("Please refrain from being a god damn card mechanic!");
        } else*/ if (c2 > c1) {
            // mathematically, leftmost bits (the 1-hot rank bits) are judged first when comparing integers, and the bit structure happens to match the order I would like to arrange the cards (higher rank first, use suit as tiebreaker)
            std::swap(c1, c2);
        }
        
        // pairs
        if (get_rank_int(c1) == get_rank_int(c2)) {
            return 168 - 14*get_rank_int(c1); // 22 = 168, 33 = 154, 44 = 140, ..., AA = 0
        // suited
        } else if (get_suit_int(c1) == get_suit_int(c2)) {
            return 13*(12-get_rank_int(c1)) + 12-get_rank_int(c2); //sanity check: for KQs -> 15. c1->11, c2->10
        // offsuit
        } else { 
            return 12-get_rank_int(c1) + 13*(12-get_rank_int(c2)); //sanity check: for KQo -> 27. c1->11, c2->10
        }
    }

    // for debugging purpose just in case
    friend std::ostream& operator<<(std::ostream& os, const Card& card) {
        os << "Card(" << get_rank_int(card.cardbits) << ", " << get_suit_int(card.cardbits) << ")";
        return os;
    }
};


struct Xoshiro256StarStar {
    /**
     * Thanks Gemini for suggesting and implementing the Xoshiro256** random number generator.
     * I hope Gemini wont mis-implement this widely documented && highly static && short algorithm
     */
    std::uint64_t s[4];
    static inline std::uint64_t rotl(const std::uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
    
    /**
     * Constructor for Xoshiro256StarStar.
     * @param seed: The seed for the random number generator.
     */
    Xoshiro256StarStar(std::uint64_t seed) {
        std::uint64_t z = seed;
        for (int i = 0; i < 4; i++) {
            z += 0x9e3779b97f4a7c15;
            std::uint64_t z_copy = z;
            z_copy = (z_copy ^ (z_copy >> 30)) * 0xbf58476d1ce4e5b9;
            z_copy = (z_copy ^ (z_copy >> 27)) * 0x94d049bb133111eb;
            s[i] = z_copy ^ (z_copy >> 31);
        }
    }

    std::uint64_t next() {
        const std::uint64_t result = rotl(s[1] * 5, 7) * 9;
        const std::uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }

    inline std::uint32_t next_int(std::uint32_t max_val) {
        // Gemini said %48 will cause modulo bias of 48/(2<<32), sounds mathematically reasonable to me so ima keep it simple
        // Deniel Lemire read me like a book, he knew exactly how I used my sacred FX-50 FH's Ran# to try my luck in Multiple Choice Questions
        std::uint64_t fx50fh_shift_decimal = next() >> 32; /*(next() & 0xFFFFFFFF);*/ // Gemini said left 32 bits are higher quality random numbers but I didnt verify
        return std::uint32_t((fx50fh_shift_decimal * max_val) >> 32); // squeeze [0, 2^32) into [0, max_val)
    }
};

struct Deck {
    /**
     * Deck class assumes that the cards are in the order of 2d, 2c, 2h, 2s, 3d ... Ks, Ad, Ac, Ah, As. 
     * The Hero's hand and Villain's hand are assumed to be taken out of the deck, 
     * and the remaining cards constitutes a 48-card deck
     * The Deck class is mainly used to generate 8 boards of 5 cards each without replacement.
     */
                /*--- BEG OF SETUP ---*/
    constexpr static int FULL_DECK[52]{
        Card(0,0), Card(0,1), Card(0,2), Card(0,3), // 2d, 2c, 2h, 2s
        Card(1,0), Card(1,1), Card(1,2), Card(1,3), // 3d, 3c, 3h, 3s
        Card(2,0), Card(2,1), Card(2,2), Card(2,3), // 4d, 4c, 4h, 4s
        Card(3,0), Card(3,1), Card(3,2), Card(3,3), // 5d, 5c, 5h, 5s
        Card(4,0), Card(4,1), Card(4,2), Card(4,3), // 6d, 6c, 6h, 6s
        Card(5,0), Card(5,1), Card(5,2), Card(5,3), // 7d, 7c, 7h, 7s
        Card(6,0), Card(6,1), Card(6,2), Card(6,3), // 8d, 8c, 8h, 8s
        Card(7,0), Card(7,1), Card(7,2), Card(7,3), // 9d, 9c, 9h, 9s
        Card(8,0), Card(8,1), Card(8,2), Card(8,3), // Td, Tc, Th, Ts
        Card(9,0), Card(9,1), Card(9,2), Card(9,3), // Jd, Jc, Jh, Js
        Card(10,0), Card(10,1), Card(10,2), Card(10,3), // Qd, Qc, Qh, Qs
        Card(11,0), Card(11,1), Card(11,2), Card(11,3), // Kd, Kc, Kh, Ks
        Card(12,0), Card(12,1), Card(12,2), Card(12,3) // Ad, Ac, Ah, As
    };
    Card cards[49]; // Gemini initially suggested 48, I think otherwise
    Deck() = delete; // no default constructor allowed
    Deck(Card h1, Card h2, Card v1, Card v2) {
        int idx = 0;
        for (int i=0;i<52;++i) {
            // Gemini is better than me in terms of branchless programming, and I can prove its soluting working by trivial
            // nvm, bro almost forgot edge case where FULL_DECK[51] is 1 of the 4 bad cards, causing idx to OOB
            Card c = FULL_DECK[i];
            cards[idx] = c;
            idx += (c != h1) & (c != h2) & (c != v1) & (c != v2); // if c is taken, let the next card to overwrite it
        }
    }
                /*--- END OF SETUP ---*/

    // The core function that Evaluator uses
    void run_8_boards(Xoshiro256StarStar& rng, int (&boards)[40]) { 
        /**
         * Run 8 boards of 5 cards each. Return a flattened array of Boards
         * 
         * Note that 8 boards are drawn simulatenously without replacement to:
         * 1. Reduce RNG overhead
         * 2. Reduce variance using blocker effect
         * 
         * And for Xoshiro256** generator, 64*4 = 256 bits > 64 bits, pbr is used
         */
        #pragma GCC unroll 8 // I hope Gemini wont trick me on this one
        for (int i=0; i<40; ++i) {
            int j = i + rng.next_int(48 - i); // random index from i to 47
            std::swap(cards[i], cards[j]);
            boards[i] = cards[i];
        }
    } 
};

#endif