/*int printf();
int malloc();

int roar(void)
{
    int a;
    return a;
}

int main(void)
{
    int *x;
    double *y;
    int i;

    x = &i;
    x[3] = 4;
    i = roar();
    printf("i: %d\n", i);
}*/

int main(void)
{
    char a, b, c, d, e[5];
    int * i;

    i = &(e[2]);

    i[0] = 5;

    printf("i[0]: %d\n", i[0]);
}
