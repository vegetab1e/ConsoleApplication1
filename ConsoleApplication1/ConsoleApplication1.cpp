#include "delegate.h"

struct Printer
{
    template<class... Types>
    void print(Types&&... arguments)
    {
        using namespace std;

        cout << __FUNCSIG__ << endl;

#if __cplusplus < 201703L
        size_t i;
        using Array = decltype(i)[];
        cout << (Array{ (cout << "decltype(args):\t", i = 0), (cout << type_name<decltype(arguments)>() << '(' << arguments << (i < sizeof...(Types) - 1 ? "), " : ")\n"), ++i)... }, '\n');
#else
        ((cout << type_name<decltype(arguments)>() << '(' << arguments << "), "), ..., (cout << '\n'));
#endif
    }

    void nonTemplatePrint(int&& arg1, long double arg2, long double arg3, void*& arg4)
    {
        using namespace std;

        cout << __FUNCSIG__ << endl;

        cout << "decltype(args):\t";
        cout << type_name<decltype(arg1)>() << '(' << arg1 << "), ";
        cout << type_name<decltype(arg2)>() << '(' << arg2 << "), ";
        cout << type_name<decltype(arg3)>() << '(' << arg3 << "), ";
        cout << type_name<decltype(arg4)>() << '(' << arg4 << ")\n\n";
    }
};

int main(int argc, char* argv[])
{
    const std::shared_ptr<Printer> printer{new Printer};
    Delegate delegate;

    int val1 = 5;
    int const& val2 = val1;
    delegate.connect(printer, static_cast<void(Printer::*)(int&&, int&&, float&&, int const&, int&, int const&)>(&Printer::print));
    delegate(1, 2, 3.14159F, 4, val1, val2);

    const char* val3 = "cstring";
    delegate.connect(printer, static_cast<void(Printer::*)(const float&&, const char*&, bool&&, long double&&)>(&Printer::print));
    delegate(-1.0F, val3, false, 2.718281828459045L);

    long double val4 = 6.62606957L;
    void* val5 = static_cast<void*>(printer.get());
    delegate.connect(printer, &Printer::nonTemplatePrint);
    delegate(1, 2.718281828459045L, val4, val5);
    delegate();

    return 0;
}
