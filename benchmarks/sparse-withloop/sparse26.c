#include <stdint.h>

/* Program with 26 "then" branches in a loop.
   Only 1 of these "then" can be taken during one iteration. */

unsigned int a[26];
int main(uint32_t seed, int limit) {
  int i;
  for(i = 1; i <= 101; i++) {
    seed = 1664525 * seed + 1013904223;
    int tcount = 0;
    if (tcount < limit && seed % 101 == 0) { a[0]++; tcount++; }
    if (tcount < limit && seed % 97 == 0) { a[1]++; tcount++; }
    if (tcount < limit && seed % 89 == 0) { a[2]++; tcount++; }
    if (tcount < limit && seed % 83 == 0) { a[3]++; tcount++; }
    if (tcount < limit && seed % 79 == 0) { a[4]++; tcount++; }
    if (tcount < limit && seed % 73 == 0) { a[5]++; tcount++; }
    if (tcount < limit && seed % 71 == 0) { a[6]++; tcount++; }
    if (tcount < limit && seed % 67 == 0) { a[7]++; tcount++; }
    if (tcount < limit && seed % 61 == 0) { a[8]++; tcount++; }
    if (tcount < limit && seed % 59 == 0) { a[9]++; tcount++; }
    if (tcount < limit && seed % 53 == 0) { a[10]++; tcount++; }
    if (tcount < limit && seed % 47 == 0) { a[11]++; tcount++; }
    if (tcount < limit && seed % 43 == 0) { a[12]++; tcount++; }
    if (tcount < limit && seed % 41 == 0) { a[13]++; tcount++; }
    if (tcount < limit && seed % 37 == 0) { a[14]++; tcount++; }
    if (tcount < limit && seed % 31 == 0) { a[15]++; tcount++; }
    if (tcount < limit && seed % 29 == 0) { a[16]++; tcount++; }
    if (tcount < limit && seed % 23 == 0) { a[17]++; tcount++; }
    if (tcount < limit && seed % 19 == 0) { a[18]++; tcount++; }
    if (tcount < limit && seed % 17 == 0) { a[19]++; tcount++; }
    if (tcount < limit && seed % 13 == 0) { a[20]++; tcount++; }
    if (tcount < limit && seed % 11 == 0) { a[21]++; tcount++; }
    if (tcount < limit && seed % 7 == 0) { a[22]++; tcount++; }
    if (tcount < limit && seed % 5 == 0) { a[23]++; tcount++; }
    if (tcount < limit && seed % 3 == 0) { a[24]++; tcount++; }
    if (tcount < limit && seed % 2 == 0) { a[25]++; tcount++; }
  }
  return a[seed % 26];
}

volatile uint32_t seed;
volatile int limit = 1;
void _start() {
  main(seed,limit);
}
