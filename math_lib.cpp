// Math library - include this in kernel.cpp with #include "math_lib.cpp"

int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n-1) + fibonacci(n-2);
}

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n-1);
}

int power(int base, int exp) {
    int result = 1;
    for (int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int sqrt_approx(int n) {
    if (n < 0) return 0;
    int x = n;
    int y = 1;
    while (x > y) {
        x = (x + y) / 2;
        y = n / x;
    }
    return x;
}

bool is_prime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return false;
    }
    return true;
}
