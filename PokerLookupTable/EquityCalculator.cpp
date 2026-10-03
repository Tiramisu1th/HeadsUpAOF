#include <fstream>
#include <iostream>
#include "Card.h"
#include "Evaluator.h"

#ifdef _OPENMP // Gemini is deploying magic with multi-threading, and I am just a mere mortal who is trying to understand the magic
    #include <omp.h>
#endif

int Evaluator::LookUpTable::flush_lookup[7937];
Evaluator::LookUpTable::FixedSizeHash16384 Evaluator::LookUpTable::unsuited_lookup;
bool Evaluator::LookUpTable::is_initialized = false;
// C++11 魅力時刻 (charismatic moment)
constexpr int Card::PRIMES[13];
constexpr int Card::INT_RANKS[13];
constexpr int Deck::FULL_DECK[52];
constexpr int Evaluator::LookUpTable::straight_flushes[10];

const int* FULL_DECK = Deck::FULL_DECK; // syntactic sugar to skip "Deck::" when calling full deck

/** 
 * Towards the end of C++ component, I would like to sincerely thank 2 courses that I took in HKUST
 * COMP2012 OOP and Data Structures, although this is a mix of love-hate, because it taught me a lot of heuristics that wouldnt work well in this specific scenario
 * ELEC2350 Introduction to Computer Organization and Design, the **clutch king** that actually includes more than assembly language
 * and the magic is that I got F and C+ respectively in my 2 attempts of ELEC2350, yet the loose fragments of memory that looked far to me back then are actually useful today
 * These courses saved me countless of AI tokens and mental bandwidth in understanding (and sometimes rebutting) AI's suggestions, especially in the following field:
 * pbv vs pbr, Class and Objects, Hashmap and probing, CPU Pipeline, Clock Cycles, branching, Caching, and more
 */

int wins[169][169]{0}; // `heroWins[hero_hand][villain_hand]` = number of times hero wins against villain
int frequency[169][169]{0}; // `frequency[hero_hand][villain_hand]` = number of times hero and villain are dealt the corresponding hands

constexpr const int TIMES_TO_SHUFFLE = 12500; // 1250*8 = 100000 boards per hero-villain hand combination, which is enough to reduce variance and get a stable result
int main() {
    Evaluator evaluator; evaluator.table.init();
    std::cout << "init finished!\n";

#ifdef _OPENMP
    std::cout << "Running in PARALLEL mode with " << omp_get_max_threads() << " threads.\n";
#else
    std::cout << "Running in SEQUENTIAL mode (OpenMP disabled).\n";
#endif

    #pragma omp parallel
    {
        int thread_id = 0;
#ifdef _OPENMP
        thread_id = omp_get_thread_num();
#endif
        Xoshiro256StarStar Pray_RNGesus = Xoshiro256StarStar(30624700+thread_id); // Human and numbers have a lot of weird reationships, look at the computer trapping so many registers inside
        int local_wins[169][169]{0}; // local wins for this thread
        int local_frequency[169][169]{0}; // local frequency for this thread
        
        int hero_card1, hero_card2, villain_card1, villain_card2;
        int hero_idx, villain_idx;
        int boards[40]; // 8 boards of 5 cards each, flattened into a 1D array
        int hero_rank, villain_rank = 7463; // worst rank possible

        #pragma omp for schedule(dynamic)
        for (int h1=0; h1<52; ++h1) {
            if (thread_id==0) std::cout<<"Running h1="<<h1<<"...";
        for (int h2=h1+1; h2<52; ++h2) {
            if (thread_id==0) std::cout<<"Running h2="<<h2<<"...\n";
        for (int v1=h1+1; v1<52; ++v1) {
        for (int v2=v1+1; v2<52; ++v2) {
            if (h2==v1 || h2==v2) continue; // hero and villain cannot share cards

            // refresh the hero and villain cards
            hero_card1 = FULL_DECK[h1]; hero_card2 = FULL_DECK[h2];
            villain_card1 = FULL_DECK[v1]; villain_card2 = FULL_DECK[v2];
            hero_idx = Card::get_13x13_index(hero_card1, hero_card2);
            villain_idx = Card::get_13x13_index(villain_card1, villain_card2);

            // instintiate the deck and run evaluation
            Deck current_deck = Deck(hero_card1, hero_card2, villain_card1, villain_card2);
            
            for (int shuffle=0; shuffle<TIMES_TO_SHUFFLE; ++shuffle) {
                current_deck.run_8_boards(Pray_RNGesus, boards);
                #pragma GCC unroll 8
                for (int board=0; board<8; ++board) {
                    hero_rank = evaluator.evaluate_once(hero_card1, hero_card2, boards[board*5], boards[board*5+1], boards[board*5+2], boards[board*5+3], boards[board*5+4]);
                    villain_rank = evaluator.evaluate_once(villain_card1, villain_card2, boards[board*5], boards[board*5+1], boards[board*5+2], boards[board*5+3], boards[board*5+4]);
                    // hero wins
                    if (hero_rank < villain_rank) local_wins[hero_idx][villain_idx] += 2;
                    // chop
                    else if (hero_rank == villain_rank) {
                        local_wins[hero_idx][villain_idx] += 1;
                        local_wins[villain_idx][hero_idx] += 1; // symmetry
                    }
                    // villain wins
                    else local_wins[villain_idx][hero_idx] += 2;
                }

                // constant 2 scores * 8 boards no matter the outcome, can be updated once outside loop instead of 8 times inside loop
                local_frequency[hero_idx][villain_idx] += 16;
                local_frequency[villain_idx][hero_idx] += 16; // symmetry
            }

        }}}}

        #pragma omp critical
        {
            for (int i = 0; i < 169; ++i) {
                for (int j = 0; j < 169; ++j) {
                    wins[i][j] += local_wins[i][j];
                    frequency[i][j] += local_frequency[i][j];
                }
            }
        }
    }

    // data post-processing, and due to the near-zero size of the loops here, ima not optimize it over safety
    float equity[169][169]{0.0f}; // `equity[hero_hand][villain_hand]` = hero's equity against villain
    for (int i=0; i<169; ++i) {
        equity[i][i] = 0.5f; // hero and villain have the same hand, so equity is always 50%
        for (int j=i+1; j<169; ++j) {
            if (frequency[i][j] + frequency[j][i] == 0) {
                equity[i][j] = 0.5f; // if hero and villain never played against each other, assume equity is 50%
                equity[j][i] = 0.5f; // symmetry
                continue;
            }
            equity[i][j] = static_cast<float>(wins[i][j]) / frequency[i][j]; // convert to percentage, one-side cast should do the job
            equity[j][i] = 1.0f - equity[i][j]; // the equity of the villain is the complement of the hero's equity
        }
    }

    // write the equity table to a file, I hope Gemini wont trick me on this machine-standard operation
    std::ofstream outfile("equity_169x169_matrix.bin", std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(reinterpret_cast<char*>(equity), sizeof(equity));
        outfile.close();
        std::cout << "Equity table written to equity_169x169_matrix.bin" << std::endl;
    } else {
        std::cerr << "Failed to open equity_169x169_matrix.bin for writing" << std::endl;
    }
    return 0;
}