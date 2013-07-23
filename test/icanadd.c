int printf();

int main(void)
{
    int a, b, c, d;
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    
    a = b - c - d * a / d;

    if(a<b)
    {
	printf("hello\n");
    }
    else
    {
	printf("good riddance\n");
    }

    printf("a: %d\n", a);
}
