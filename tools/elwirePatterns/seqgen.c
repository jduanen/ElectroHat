/*
* Tool to generate sequences of patterns for the EL wire library
*/

#include <stdio.h>
#include <assert.h>


#define SEQUENCES_VERSION   "1.0.0"

#define NUM_EL_WIRES            8
#define MAX_SEQUENCES           128

#include "seqdefs.h"

typedef struct {
    unsigned short  offset;
    unsigned short  length;
} SequenceDefinition;


SequenceDefinition sequenceDefinitions[MAX_SEQUENCES];
unsigned char patterns[NUM_SEQUENCES * MAX_SEQUENCE_LENGTH];


int main() {
    int patNum;
    int indx = 0;
    for (int seqNum = 0; (seqNum < NUM_SEQUENCES); seqNum++) {
        sequenceDefinitions[seqNum].offset = indx;
        for (patNum = 0; (patNum < MAX_SEQUENCE_LENGTH); patNum++) {
            if (sequences[seqNum][patNum][0] == 0) {
                break;
            }
            unsigned char bits = 0;
            for (int wireNum = 0; (wireNum < NUM_EL_WIRES); wireNum++) {
                bits = (bits << 1) | (sequences[seqNum][patNum][wireNum] != ' ');
            }
            patterns[indx++] = bits;
        }
        sequenceDefinitions[seqNum].length = patNum;
    }

    printf("/* Auto-generated EL wire pattern sequences file */\n\n");
    printf("#ifndef SEQUENCES_H\n");
    printf("#define SEQUENCES_H\n\n");

    printf("#define SEQUENCES_VERSION   \"%s\"\n", SEQUENCES_VERSION);
    printf("#define NUM_EL_WIRES        %d\n", NUM_EL_WIRES);
    printf("#define NUM_SEQUENCES       %d\n\n", NUM_SEQUENCES);

    printf("typedef struct {\n");
    printf("    unsigned short offset;\n");
    printf("    unsigned short length;\n");
    printf("} SequenceDefinition;\n\n");

    printf("SequenceDefinition sequenceDefinitions[] = {\n");
    for (int seqNum = 0; (seqNum < NUM_SEQUENCES); seqNum++) {
        printf("    { 0x%04X, 0x%04X },\n", sequenceDefinitions[seqNum].offset, sequenceDefinitions[seqNum].length);
    }
    printf("};\n\n");
    printf("byte patterns[] = {\n    ");
    for (int i = 0; (i < sizeof(patterns)); i++) {
        printf("0x%02X, ", patterns[i]);
        if (((i + 1) % 32) == 0) {
            printf("\n    ");
        }
    }
    printf("};\n");

    printf("#endif /*FONTS_H*/\n");
    return 0;
}
