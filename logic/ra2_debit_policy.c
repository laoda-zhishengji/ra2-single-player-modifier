#include <stdio.h>

static int apply_money_delta(int current, int delta, int is_player, int no_decrease) {
    if (no_decrease && is_player && delta < 0) return current;
    return current + delta;
}

static int expect(const char *name, int actual, int wanted) {
    if (actual != wanted) {
        printf("FAIL %s: got %d, expected %d\n", name, actual, wanted);
        return 0;
    }
    printf("PASS %s\n", name);
    return 1;
}

int main(void) {
    int ok = 1;
    ok &= expect("player purchase is blocked", apply_money_delta(2400, -500, 1, 1), 2400);
    ok &= expect("player income remains enabled", apply_money_delta(2400, 500, 1, 1), 2900);
    ok &= expect("AI debit is untouched", apply_money_delta(2400, -500, 0, 1), 1900);
    ok &= expect("disabled feature is untouched", apply_money_delta(2400, -500, 1, 0), 1900);
    return ok ? 0 : 1;
}
