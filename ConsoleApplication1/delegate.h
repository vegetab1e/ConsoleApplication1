#pragma once

#include <iostream>
#include <iomanip>

#include <memory>
#include <utility>

#include <tuple>

#include "typename.h"

struct IArguments
{
    virtual ~IArguments() = 0;
};

inline IArguments::~IArguments() = default;

#ifdef UNDER_CONSTRUCTION
template<class... Types>
struct Arguments: IArguments
{
    explicit Arguments(Types&&... arguments) noexcept
        : tuple_(std::forward_as_tuple(std::forward<Types>(arguments)...))
    {
    }

    std::tuple<Types&&...> tuple_;
};
#else
template<class... Types>
struct Arguments: IArguments
{
    explicit Arguments(std::tuple<Types...>&& tuple) noexcept
        : tuple_(tuple)
    {
    }

    std::tuple<Types...> tuple_;
};
#endif


class ICallback
{
public:
#ifdef UNDER_CONSTRUCTION
    virtual void invokeMethod(const IArguments&) const = 0;
#else
    virtual void invokeMethod(IArguments&&) const = 0;
#endif
    virtual ~ICallback() = 0;
};

inline ICallback::~ICallback() = default;


#if __cplusplus < 201402L
template<std::size_t...>
struct IndexSequence;

template<std::size_t N, std::size_t... S>
struct MakeIndexSequence: MakeIndexSequence<N - 1, N - 1, S...>
{
};

template<std::size_t... S>
struct MakeIndexSequence<0, S...>
{
    using Type = IndexSequence<S...>;
};
#endif


template<class, class>
class Callback;

template<class Object, class... Types>
class Callback<Object, void (Object::*)(Types...)>: public ICallback
{
    using Method = void (Object::*)(Types...);

public:
    Callback(const std::shared_ptr<Object>& object, Method method) noexcept
        : object_(object)
        , method_(method)
    {
    }

#ifdef UNDER_CONSTRUCTION
    void invokeMethod(const IArguments& arguments) const override
    try
    {
#if __cplusplus < 201402L
        invokeMethod(dynamic_cast<const Arguments<Types...>&>(arguments).tuple_, typename MakeIndexSequence<sizeof...(Types)>::Type());
#else
        invokeMethod(dynamic_cast<const Arguments<Types...>&>(arguments).tuple_, std::index_sequence_for<Types...>());
#endif
    }
    catch (const std::bad_cast& exception)
    {
#ifdef _DEBUG
        std::cout << __FUNCSIG__ << std::endl;
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
        std::cout << exception.what() << "\n\n";
#endif
    }
#else
    void invokeMethod(IArguments&& arguments) const override
    try
    {
#if __cplusplus < 201402L
        invokeMethod(dynamic_cast<Arguments<std::decay_t<Types>...>&&>(arguments), typename MakeIndexSequence<sizeof...(Types)>::Type());
#else
        invokeMethod(dynamic_cast<Arguments<std::decay_t<Types>...>&&>(arguments), std::index_sequence_for<Types...>());
#endif
    }
    catch (const std::bad_cast& exception)
    {
#ifdef _DEBUG
        std::cout << __FUNCSIG__ << std::endl;
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
        std::cout << exception.what() << "\n\n";
#endif
    }
#endif

private:
#ifdef UNDER_CONSTRUCTION
    template<class Tuple, std::size_t... indexes>
#if __cplusplus < 201402L
    void invokeMethod(const Tuple& arguments, IndexSequence<indexes...>) const
#else
    void invokeMethod(const Tuple& arguments, std::index_sequence<indexes...>) const
#endif
    {
        const auto shared_pointer{object_.lock()};
        if (auto raw_pointer = shared_pointer.get())
            (raw_pointer->*method_)(std::forward<std::tuple_element_t<indexes, Tuple>>(std::get<indexes>(arguments))...);
    }
#else
    template<std::size_t... indexes>
#if __cplusplus < 201402L
    void invokeMethod(Arguments<std::decay_t<Types>...>&& arguments, IndexSequence<indexes...>) const
#else
    void invokeMethod(Arguments<std::decay_t<Types>...>&& arguments, std::index_sequence<indexes...>) const
#endif
    {
        const auto shared_pointer{object_.lock()};
        if (auto raw_pointer = shared_pointer.get())
            (raw_pointer->*method_)(std::forward<Types>(std::get<indexes>(arguments.tuple_))...);
    }
#endif

    const std::weak_ptr<Object> object_;
    const Method method_;
};

class Delegate final
{
public:
    template<class Object, class Method>
    void connect(const std::shared_ptr<Object>& object, Method method)
    {
        callback_ = std::make_unique<Callback<Object, Method>>(object, method);
    }

    template<class... Types>
    void operator()(Types&&... args) const
    {
        if (const ICallback* raw_pointer = callback_.get())
        {
#ifdef _DEBUG
            using namespace std;

            cout << __FUNCSIG__ << endl;

            if (!!sizeof...(Types))
            {
                const auto default_precision{cout.precision()};

                size_t i;
                using Array = decltype(i)[];
                const Array column_sizes = { 0, type_name<decltype(args)>().size()... };
                auto info = [&](const string& type_name, const auto& value){
                    using Type = decay_t<decltype(value)>;
                    if (is_floating_point_v<Type>)
                        cout << fixed << setprecision(numeric_limits<Type>::max_digits10);
                    cout << setw(column_sizes[i]) << type_name << '(' << value << (i < sizeof...(Types) ? "), " : ")\n");
                };
                (void)Array{ (cout << "Types:\t\t",        i = 1), (info(type_name<Types>(),                          args), ++i)... };
                (void)Array{ (cout << "decay_t<Types>:\t", i = 1), (info(type_name<decay_t<Types>>(),                 args), ++i)... };
                (void)Array{ (cout << "decltype(args):\t", i = 1), (info(type_name<decltype(args)>(),                 args), ++i)... };
                (void)Array{ (cout << "forward<Types>:\t", i = 1), (info(type_name<decltype(forward<Types>(args))>(), args), ++i)... };

                cout << defaultfloat << setprecision(default_precision) << endl;
            }
#endif

#ifdef UNDER_CONSTRUCTION
            raw_pointer->invokeMethod(Arguments<Types...>{std::forward<Types>(args)...});
#else
            raw_pointer->invokeMethod(Arguments<std::decay_t<Types>...>{std::make_tuple(std::forward<Types>(args)...)});
#endif
        }
    }

private:
    std::unique_ptr<ICallback> callback_;
};
