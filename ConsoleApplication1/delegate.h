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

    explicit Arguments(std::tuple<Types...>&& tuple) noexcept
        : tuple_(tuple)
    {
    }

    std::tuple<Types...> tuple_;
};

template<class... Types>
auto makeArguments(Types&&... arguments) noexcept
{
    return Arguments<Types...>{std::forward<Types>(arguments)...};
}
#else
template<class... Types>
struct ValueArguments: IArguments
{
    explicit ValueArguments(std::tuple<Types...>&& tuple) noexcept
        : tuple_(tuple)
    {
    }

    std::tuple<Types...> tuple_;
};

template<class... Types>
struct ReferenceArguments: IArguments
{
    explicit ReferenceArguments(Types&... arguments) noexcept
        : tuple_(std::tie(arguments...))
    {
    }

    const std::tuple<Types&...> tuple_;
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
template<size_t...>
struct IndexSequence;

template<size_t N, size_t... S>
struct MakeIndexSequence: MakeIndexSequence<N - 1, N - 1, S...>
{
};

template<size_t... S>
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
        invokeMethod(dynamic_cast<const Arguments<Types...>&>(arguments), typename MakeIndexSequence<sizeof...(Types)>::Type());
#else
        invokeMethod(dynamic_cast<const Arguments<Types...>&>(arguments), std::index_sequence_for<Types...>());
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
        invokeMethod(dynamic_cast<ValueArguments<std::decay_t<Types>...>&&>(arguments), typename MakeIndexSequence<sizeof...(Types)>::Type());
#else
        invokeMethod(dynamic_cast<ValueArguments<std::decay_t<Types>...>&&>(arguments), std::index_sequence_for<Types...>());
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
    template<size_t... indexes>
#if __cplusplus < 201402L
    void invokeMethod(const Arguments<Types...>& arguments, IndexSequence<indexes...>) const
#else
    void invokeMethod(const Arguments<Types...>& arguments, std::index_sequence<indexes...>) const
#endif
    {
        const auto shared_pointer{object_.lock()};
        if (auto raw_pointer = shared_pointer.get())
            (raw_pointer->*method_)(std::forward<std::tuple_element_t<indexes, std::remove_reference_t<decltype((arguments.tuple_))>>>(std::get<indexes>(arguments.tuple_))...);
    }
#else
    template<size_t... indexes>
#if __cplusplus < 201402L
    void invokeMethod(ValueArguments<std::decay_t<Types>...>&& arguments, IndexSequence<indexes...>) const
#else
    void invokeMethod(ValueArguments<std::decay_t<Types>...>&& arguments, std::index_sequence<indexes...>) const
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

#ifdef UNDER_CONSTRUCTION
#if __cplusplus < 202002L
template<class Type>
using RemoveCVRef = std::remove_cv_t<std::remove_reference_t<Type>>;
#else
template<class Type>
using RemoveCVRef = std::remove_cvref_t<Type>;
#endif

template<class Type>
std::enable_if_t<std::is_lvalue_reference_v<Type>, std::add_lvalue_reference_t<RemoveCVRef<Type>>>
removeConstVolatile() noexcept;

template<class Type>
std::enable_if_t<std::is_rvalue_reference_v<Type>, std::add_rvalue_reference_t<RemoveCVRef<Type>>>
removeConstVolatile() noexcept;

template<class Type>
std::enable_if_t<not std::is_reference_v<Type>, std::remove_cv_t<Type>>
removeConstVolatile() noexcept;
#endif

class Delegate final
{
public:
    template<class Object, class... Types>
    void connect(const std::shared_ptr<Object>& object, void (Object::*method)(Types...))
    {
#ifdef UNDER_CONSTRUCTION
        using Method = void (Object::*)(decltype(removeConstVolatile<Types>())...);
        callback_ = std::make_unique<Callback<Object, Method>>(object, reinterpret_cast<Method>(method));
#else
        callback_ = std::make_unique<Callback<Object, void (Object::*)(Types...)>>(object, method);
#endif
    }

    template<class... Types>
    void operator()(Types&&... args) const
    {
        if (const ICallback* raw_pointer = callback_.get())
        {
            using namespace std;
#ifdef _DEBUG
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
            raw_pointer->invokeMethod(makeArguments<Types&&...>(forward<Types>(args)...));
#else
            raw_pointer->invokeMethod(ValueArguments<decay_t<Types>...>{make_tuple(forward<Types>(args)...)});
#endif
        }
    }

private:
    std::unique_ptr<ICallback> callback_;
};
