#include <fstream>
#include <iostream>




float matchup_frequency[169][169]; // `frequency[hero_hand][villain_hand]` = number of times hero and villain are dealt the corresponding hands
constexpr int FREQUENCY[169] { 6,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4, 4,
                              12,  6,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4, 4,
                              12, 12,  6,  4,  4,  4,  4,  4,  4,  4,  4,  4, 4,
                              12, 12, 12,  6,  4,  4,  4,  4,  4,  4,  4,  4, 4,
                              12, 12, 12, 12,  6,  4,  4,  4,  4,  4,  4,  4, 4,
                              12, 12, 12, 12, 12,  6,  4,  4,  4,  4,  4,  4, 4,
                              12, 12, 12, 12, 12, 12,  6,  4,  4,  4,  4,  4, 4,
                              12, 12, 12, 12, 12, 12, 12,  6,  4,  4,  4,  4, 4,
                              12, 12, 12, 12, 12, 12, 12, 12,  6,  4,  4,  4, 4,
                              12, 12, 12, 12, 12, 12, 12, 12, 12,  6,  4,  4, 4,
                              12, 12, 12, 12, 12, 12, 12, 12, 12, 12,  6,  4, 4,
                              12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,  6, 4,
                              12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 6};

int main() {
    int row,col = 0;
    int highcard, lowcard = 0;
    int current_frequency[13][13] = {0}; // current matchup frequency for a given hero hand, to be normalized to 1.0
    for (int hand_idx=0; hand_idx<169; ++hand_idx) {
        row = hand_idx/13; col = hand_idx%13;
        // np.copy(FREQUENCY).reshape(13, 13) to get the base frequency matrix
        for (int deep_copy_row=0; deep_copy_row<13; ++deep_copy_row) for (int deep_copy_col=0; deep_copy_col<13; ++deep_copy_col) current_frequency[deep_copy_row][deep_copy_col] = FREQUENCY[deep_copy_row*13 + deep_copy_col];
        
        // no vectorized operatrations in C++11 >.<
        if (row==col) { // pocket pairs
            current_frequency[row][col] = 4; // pocket pair requires further operations to be tuned down to 1
            for (int i=0; i<13; ++i) {
                current_frequency[row][i] /= 2; // blocks half of the combos in the same row
                current_frequency[i][col] /= 2; // blocks half of the combos in the same column
            }
        } else if (row<col) { // suited hands
            // bfor suited hands, row represents the high card and col represents the low card
            highcard = row; lowcard = col;
            for (int i=0; i<13; ++i) {
                // blocks 1/4 of the combos in both high card and low card
                current_frequency[highcard][i] /= 4; current_frequency[highcard][i] *= 3;
                current_frequency[lowcard][i] /= 4; current_frequency[lowcard][i] *= 3;
                current_frequency[i][highcard] /= 4; current_frequency[i][highcard] *= 3;
                current_frequency[i][lowcard] /= 4; current_frequency[i][lowcard] *= 3;
            }
            // note that themselves, offsuit hands, and pocket pair combos are screwed, so requires a fix
            current_frequency[row][col] = 3; // suited counterpart has 3 combos remaining
            current_frequency[col][row] = 6; // offsuit counterpart has 6 combos remaining
            current_frequency[row][row] = current_frequency[col][col] = 3; // affected pocket pairs counterpart has 3 combos remaining
        } else { // offsuit hands
            // for offsuit hands, row represents the high card and col represents the low card
            highcard = col; lowcard = row;
            for (int i=0; i<13; ++i) {
                // blocks 1/4 of the combos in both high card and low card
                current_frequency[highcard][i] /= 4; current_frequency[highcard][i] *= 3;
                current_frequency[lowcard][i] /= 4; current_frequency[lowcard][i] *= 3;
                current_frequency[i][highcard] /= 4; current_frequency[i][highcard] *= 3;
                current_frequency[i][lowcard] /= 4; current_frequency[i][lowcard] *= 3;
            }
            // note that themselves, suited hands, and pocket pair combos are screwed, so requires a fix
            current_frequency[row][col] = 7; // offsuit counterpart has 7 combos remaining
            current_frequency[col][row] = 2; // suited counterpart has 2 combos remaining
            current_frequency[row][row] = current_frequency[col][col] = 3; // affected pocket pairs counterpart has 3 combos remaining
        }
        if (hand_idx==40) std::cout << "current_frequency for KJo:\n";
        // flatten and L1 normalize to match realistic distribution of hands
        for (int deep_copy_again=0; deep_copy_again<169;++deep_copy_again) {
            if (hand_idx==40) std::cout << static_cast<float>(current_frequency[deep_copy_again/13][deep_copy_again%13]) << " ";
            matchup_frequency[hand_idx][deep_copy_again] = static_cast<float>(current_frequency[deep_copy_again/13][deep_copy_again%13])/1225.0f;
        }
    }
    /**
     * Case 1: TT (hand_idx = 56), row = 56/13 = 4, col = 56%13 = 4
     * 1. current_frequency[:,:] successfully resetted correctly
     * 2. current_frequency[4][4] = 4, current_frequency[4,:] /= 2, current_frequency[:,4] /= 2
     * 3. current_frequency[4][4] = 1, current_frequency[suiteds] = 2, current_frequency[offsuits] = 6
     * 
     * Case 2: A5s (hand_idx = 9), row = 9/13 = 0, col = 9%13 = 9
     * 1. current_frequency[:,:] successfully resetted correctly
     * 2. row<col -> suited hands, highcard = 0, lowcard = 9, correct so far
     * 3. current_frequency[0,:] *= 3/4, current_frequency[9,:] *= 3/4, current_frequency[:,0] *= 3/4, current_frequency[:,9] *= 3/4
     * 4. current_frequency[0][9] = 3, current_frequency[9][0] = 6, current_frequency[0][0] = current_frequency[9][9] = 3
    */

    // write the frequency table to a file, I hope Gemini wont trick me on this machine-standard operation
    std::ofstream outfile("frequency_169x169_matrix.bin", std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(reinterpret_cast<char*>(matchup_frequency), sizeof(matchup_frequency));
        outfile.close();
        std::cout << "Frequency table written to frequency_169x169_matrix.bin" << std::endl;
    } else {
        std::cerr << "Failed to open frequency_169x169_matrix.bin for writing" << std::endl;
    }
    return 0;
}