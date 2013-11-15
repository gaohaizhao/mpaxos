
void b() {
    int i = 0;
    int j = 0;
    for (i = 0; i < 10000; i++) {
        j += i;
    }
}

void a() {
    int i = 0;
    int j = 0;
    for (i = 0; i < 10000; i++) {
        j += i; 
    }

}


int main() {
    int i = 0;
    for (i = 0; i < 100000; i++) {
        a();
        b();
        b();
    }
    exit(0);
}
