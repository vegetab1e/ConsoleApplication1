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

    const std::tuple<Types&&...> tuple_;
};
#else
template<class... Types>
class Arguments final: public IArguments
{
public:
    using Tuple = std::tuple<Types...>;

    Arguments(Arguments const&) = delete;
    Arguments(Arguments&&) = default;
    Arguments& operator=(Arguments const&) = delete;
    Arguments& operator=(Arguments&&) = default;

    explicit Arguments(Tuple&& tuple) noexcept
        : tuple_(std::move(tuple))
    {
    }

    const Tuple& getData() const & noexcept
    {
        return tuple_;
    }

    Tuple&& getData() && noexcept
    {
        return std::move(tuple_);
    }

private:
    Tuple tuple_;
};
#endif


class ICallback
{
public:
#ifdef UNDER_CONSTRUCTION
    virtual void invokeMethod(const IArguments&) const noexcept = 0;
#else
    virtual void invokeMethod(IArguments&&) const noexcept = 0;
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
class Callback<Object, void (Object::*)(Types...)> final: public ICallback
{
    using Method = void (Object::*)(Types...);

public:
    Callback(const std::shared_ptr<Object>& object, Method method) noexcept
        : object_(object)
        , method_(method)
    {
    }

#ifdef UNDER_CONSTRUCTION
    void invokeMethod(const IArguments& arguments) const noexcept override
    try
    {
#if __cplusplus < 201402L
        invokeMethod(dynamic_cast<const Arguments<Types...>&>(arguments).tuple_, typename MakeIndexSequence<sizeof...(Types)>::Type());
#else
        invokeMethod(dynamic_cast<const Arguments<Types...>&>(arguments).tuple_, std::index_sequence_for<Types...>());
#endif
    }
#else
    void invokeMethod(IArguments&& arguments) const noexcept override
    try
    {
#if __cplusplus < 201402L
        invokeMethod(dynamic_cast<Arguments<std::decay_t<Types>...>&&>(arguments).getData(), typename MakeIndexSequence<sizeof...(Types)>::Type());
#else
        invokeMethod(dynamic_cast<Arguments<std::decay_t<Types>...>&&>(arguments).getData(), std::index_sequence_for<Types...>());
#endif
    }
#endif
    catch (const std::bad_cast& exception)
    {
#ifdef _DEBUG
        std::cout << __FUNCSIG__ << std::endl;
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
        std::cout << exception.what() << "\n\n";
#endif
    }

private:
#ifdef UNDER_CONSTRUCTION
    template<class Tuple, std::size_t... indexes>
#if __cplusplus < 201402L
    void invokeMethod(const Tuple& arguments, IndexSequence<indexes...>) const noexcept
#else
    void invokeMethod(const Tuple& arguments, std::index_sequence<indexes...>) const noexcept
#endif
    try
    {
        const auto shared_pointer{object_.lock()};
        const auto raw_pointer{shared_pointer.get()};
        if (!!raw_pointer && !!method_)
            (raw_pointer->*method_)(std::forward<std::tuple_element_t<indexes, Tuple>>(std::get<indexes>(arguments))...);
    }
#else
    template<class Tuple, std::size_t... indexes>
#if __cplusplus < 201402L
    void invokeMethod(Tuple&& arguments, IndexSequence<indexes...>) const noexcept
#else
    void invokeMethod(Tuple&& arguments, std::index_sequence<indexes...>) const noexcept
#endif
    try
    {
        const auto shared_pointer{object_.lock()};
        const auto raw_pointer{shared_pointer.get()};
        if (!!raw_pointer && !!method_)
            (raw_pointer->*method_)(std::forward<Types>(std::get<indexes>(arguments))...);
    }
#endif
    catch (...)
    {
        std::cout << "typeid(method): " << typeid(method_).name() << '\n';
        std::cout << "typeid(arguments): " << typeid(arguments).name() << '\n';
        std::cout << "Exception catched!" << "\n\n";
    }

    const std::weak_ptr<Object> object_;
    const Method method_;
};

class Delegate final
{
public:
    template<class Object, class Method>
    void connect(const std::shared_ptr<Object>& object, Method method)
    {
        if (object && !!method)
            callback_ = std::make_unique<Callback<Object, Method>>(object, method);
    }

    template<class... Types>
    void operator()(Types&&... arguments) const
    {
        if (const ICallback* raw_pointer = callback_.get())
        {
#ifdef _DEBUG
            std::cout << __FUNCSIG__ << std::endl;

            if (!!sizeof...(Types))
            {
                using namespace std;

                const auto default_precision{cout.precision()};

                size_t i;
                using Array = decltype(i)[];
                const Array column_sizes = { 0, type_name<decltype(arguments)>().size()... };
                auto info = [&](const string& type_name, const auto& value){
                    using Type = decay_t<decltype(value)>;
                    if (is_floating_point_v<Type>)
                        cout << fixed << setprecision(numeric_limits<Type>::max_digits10);
                    cout << setw(column_sizes[i]) << type_name << '(' << value << (i < sizeof...(Types) ? "), " : ")\n");
                };
                (void)Array{ (cout << "Types:\t\t",        i = 1), (info(type_name<Types>(),                               arguments), ++i)... };
                (void)Array{ (cout << "decay_t<Types>:\t", i = 1), (info(type_name<decay_t<Types>>(),                      arguments), ++i)... };
                (void)Array{ (cout << "decltype(args):\t", i = 1), (info(type_name<decltype(arguments)>(),                 arguments), ++i)... };
                (void)Array{ (cout << "forward<Types>:\t", i = 1), (info(type_name<decltype(forward<Types>(arguments))>(), arguments), ++i)... };

                cout << defaultfloat << setprecision(default_precision) << endl;
            }
#endif

#ifdef UNDER_CONSTRUCTION
            raw_pointer->invokeMethod(Arguments<Types...>{std::forward<Types>(arguments)...});
#else
            raw_pointer->invokeMethod(Arguments<std::decay_t<Types>...>{std::make_tuple(std::forward<Types>(arguments)...)});
#endif
        }
    }

private:
    std::unique_ptr<ICallback> callback_;
};
