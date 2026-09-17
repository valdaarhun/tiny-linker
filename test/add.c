static int increment(int b) {
    int cnt = 0;
    for(int i = 0; i < b; i++)
        cnt++;
    return cnt;
}

int add(int a, int b) {
    return a + increment((a + b) >> 1);
}
