/* Program with 10+1 (cheap?) feasible path.
   Contains 2^10 syntactic paths. */
int main(int argc, char *argv[]) {
  int x = 0;
  int tt = 0;
  int c1, c2, c3, c4, c5, c6, c7, c8, c9, c10;
  if (c1 && !tt) { x=1; tt=1; }
  if (c2 && !tt) { x=2; tt=1; }
  if (c3 && !tt) { x=3; tt=1; }
  if (c4 && !tt) { x=4; tt=1; }
  if (c5 && !tt) { x=5; tt=1; }
  if (c6 && !tt) { x=6; tt=1; }
  if (c7 && !tt) { x=7; tt=1; }
  if (c8 && !tt) { x=8; tt=1; }
  if (c9 && !tt) { x=9; tt=1; }
  if (c10 && !tt) { x=10; tt=1; }
  return x;
}
