
// Copyright (c) 2015 Noah Lopez
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#ifndef MSEPOLY_H_
#define MSEPOLY_H_

//include "mseprimitives.h"
#include "mseregistered.h"
#include "mserelaxedregistered.h"
#include "mserefcounting.h"
#include "msescope.h"
#include "msemstdvector.h"
#include "mseasyncshared.h"
#include "mseany.h"
#include "msemstdarray.h"
#include <memory>
#include <iostream>
#include <utility>
#include <cassert>

#include <typeinfo>
#include <type_traits>
#include <new>

/* for the test functions */
#include <map>
#include <string>

#ifdef MSE_CUSTOM_THROW_DEFINITION
#include <iostream>
#define MSE_THROW(x) MSE_CUSTOM_THROW_DEFINITION(x)
#else // MSE_CUSTOM_THROW_DEFINITION
#define MSE_THROW(x) throw(x)
#endif // MSE_CUSTOM_THROW_DEFINITION

#ifdef MSE_SAFER_SUBSTITUTES_DISABLED
#define MSE_POLYPOINTER_DISABLED
#endif /*MSE_SAFER_SUBSTITUTES_DISABLED*/

namespace mse {

#ifdef MSE_POLYPOINTER_DISABLED
#else /*MSE_POLYPOINTER_DISABLED*/
#endif /*MSE_POLYPOINTER_DISABLED*/

#if defined(_MSC_VER)
#pragma warning(disable:4503)
#endif

	/* The original variant code came from: https://gist.github.com/tibordp/6909880 */
	template <size_t arg1, size_t ... others>
	struct static_max;

	template <size_t arg>
	struct static_max<arg>
	{
		static const size_t value = arg;
	};

	template <size_t arg1, size_t arg2, size_t ... others>
	struct static_max<arg1, arg2, others...>
	{
		static const size_t value = arg1 >= arg2 ? static_max<arg1, others...>::value :
			static_max<arg2, others...>::value;
	};

	template<typename... Ts>
	struct tdp_variant_helper;

	template<typename F, typename... Ts>
	struct tdp_variant_helper<F, Ts...> {
		inline static void destroy(size_t id, void * data)
		{
			if (id == typeid(F).hash_code())
				reinterpret_cast<F*>(data)->~F();
			else
				tdp_variant_helper<Ts...>::destroy(id, data);
		}

		inline static void move(size_t old_t, void * old_v, void * new_v)
		{
			if (old_t == typeid(F).hash_code())
				::new (new_v) F(std::move(*reinterpret_cast<F*>(old_v)));
			else
				tdp_variant_helper<Ts...>::move(old_t, old_v, new_v);
		}

		inline static void copy(size_t old_t, const void * old_v, void * new_v)
		{
			if (old_t == typeid(F).hash_code())
				::new (new_v) F(*reinterpret_cast<const F*>(old_v));
			else
				tdp_variant_helper<Ts...>::copy(old_t, old_v, new_v);
		}
	};

	template<> struct tdp_variant_helper<> {
		inline static void destroy(size_t id, void * data) { }
		inline static void move(size_t old_t, void * old_v, void * new_v) { }
		inline static void copy(size_t old_t, const void * old_v, void * new_v) { }
	};

	template<typename... Ts>
	struct tdp_variant {
	protected:
		static const size_t data_size = static_max<sizeof(Ts)...>::value;
		static const size_t data_align = static_max<alignof(Ts)...>::value;

		using data_t = typename std::aligned_storage<data_size, data_align>::type;

		using helper_t = tdp_variant_helper<Ts...>;

		static inline size_t invalid_type() {
			return typeid(void).hash_code();
		}

		size_t type_id;
		data_t data;
	public:
		tdp_variant() : type_id(invalid_type()) {   }

		tdp_variant(const tdp_variant<Ts...>& old) : type_id(old.type_id)
		{
			helper_t::copy(old.type_id, &old.data, &data);
		}

		tdp_variant(tdp_variant<Ts...>&& old) : type_id(old.type_id)
		{
			helper_t::move(old.type_id, &old.data, &data);
		}

		// Serves as both the move and the copy asignment operator.
		tdp_variant<Ts...>& operator= (tdp_variant<Ts...> old)
		{
			std::swap(type_id, old.type_id);
			std::swap(data, old.data);

			return *this;
		}

		template<typename T>
		bool is() {
			return (type_id == typeid(T).hash_code());
		}

		bool valid() {
			return (type_id != invalid_type());
		}

		template<typename T, typename... Args>
		void set(Args&&... args)
		{
			// First we destroy the current contents    
			helper_t::destroy(type_id, &data);
			::new (&data) T(std::forward<Args>(args)...);
			type_id = typeid(T).hash_code();
		}

		template<typename T>
		T& get()
		{
			// It is a dynamic_cast-like behaviour
			if (type_id == typeid(T).hash_code())
				return *reinterpret_cast<T*>(&data);
			else
				MSE_THROW(std::bad_cast());
		}

		~tdp_variant() {
			helper_t::destroy(type_id, &data);
		}
	};

	template<typename... Ts>
	struct tdp_pointer_variant_helper;

	template<typename F, typename... Ts>
	struct tdp_pointer_variant_helper<F, Ts...> {
		inline static void* arrow_operator(size_t id, const void * data) {
			if (id == typeid(F).hash_code()) {
				return (reinterpret_cast<const F*>(data))->operator->();
			}
			else {
				return tdp_pointer_variant_helper<Ts...>::arrow_operator(id, data);
			}
		}

		inline static const void* const_arrow_operator(size_t id, const void * data) {
			if (id == typeid(F).hash_code()) {
				return (reinterpret_cast<const F*>(data))->operator->();
			}
			else {
				return tdp_pointer_variant_helper<Ts...>::const_arrow_operator(id, data);
			}
		}
	};

	template<> struct tdp_pointer_variant_helper<> {
		inline static void* arrow_operator(size_t id, const void * data) { return nullptr; }
		inline static const void* const_arrow_operator(size_t id, const void * data) { return nullptr; }
	};

	template<typename... Ts>
	struct tdp_pointer_variant : public tdp_variant<Ts...> {
	protected:
		using pointer_helper_t = tdp_pointer_variant_helper<Ts...>;
	public:
		using tdp_variant<Ts...>::tdp_variant;

		void* arrow_operator() const {
			return pointer_helper_t::arrow_operator((*this).type_id, &((*this).data));
		}
		const void* const_arrow_operator() const {
			return pointer_helper_t::const_arrow_operator((*this).type_id, &((*this).data));
		}
	};

	template <typename _Ty>
	class TXScopeAnyConstPointer;

	template <typename _Ty>
	class TCommonPointerInterface {
	public:
		virtual ~TCommonPointerInterface() {}
		virtual _Ty& operator*() const = 0;
		virtual _Ty* operator->() const = 0;
	};

	template <typename _Ty, typename _TPointer1>
	class TCommonizedPointer : public TCommonPointerInterface<_Ty> {
	public:
		TCommonizedPointer(const _TPointer1& pointer) : m_pointer(pointer) {}
		virtual ~TCommonizedPointer() {}

		_Ty& operator*() const {
			return (*m_pointer);
		}
		_Ty* operator->() const {
			return m_pointer.operator->();
		}

		_TPointer1 m_pointer;
	};

	template <typename _Ty>
	class TXScopeAnyPointer {
	public:
		TXScopeAnyPointer(const TXScopeAnyPointer& src) : m_any_pointer(src.m_any_pointer) {}

		template <typename _TPointer1>
		TXScopeAnyPointer(const _TPointer1& pointer) : m_any_pointer(TCommonizedPointer<_Ty, _TPointer1>(pointer)) {}

		_Ty& operator*() const {
			return (*(*common_pointer_interface_const_ptr()));
		}
		_Ty* operator->() const {
			return common_pointer_interface_const_ptr()->operator->();
		}

	protected:
		TXScopeAnyPointer<_Ty>& operator=(const TXScopeAnyPointer<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopeAnyPointer<_Ty>* operator&() { return this; }
		const TXScopeAnyPointer<_Ty>* operator&() const { return this; }

		const TCommonPointerInterface<_Ty>* common_pointer_interface_const_ptr() const {
			auto retval = reinterpret_cast<const TCommonPointerInterface<_Ty>*>(m_any_pointer.storage_address());
			assert(nullptr != retval);
			return retval;
		}

		mse::any m_any_pointer;
	};

	template <typename _Ty>
	class TAnyPointer : public TXScopeAnyPointer<_Ty> {
	public:
		TAnyPointer(const TAnyPointer& src) : TXScopeAnyPointer<_Ty>(src) {}

		template <typename _TPointer1
#ifndef MSE_SCOPEPOINTER_DISABLED
			, class = typename std::enable_if<
			(!std::is_convertible<_TPointer1, TXScopeFixedPointer<_Ty>>::value) 
			&& (!std::is_convertible<_TPointer1, TXScopeFixedConstPointer<_Ty>>::value)
			&& (!std::is_same<_TPointer1, TXScopeAnyPointer<_Ty>>::value)
			&& (!std::is_same<_TPointer1, TXScopeAnyConstPointer<_Ty>>::value)
			, void>::type
#endif // !MSE_SCOPEPOINTER_DISABLED
		>
			TAnyPointer(const _TPointer1& pointer) : TXScopeAnyPointer<_Ty>(pointer) {}

	protected:
		TAnyPointer<_Ty>& operator=(const TAnyPointer<_Ty>& _Right_cref) = delete;

		TAnyPointer<_Ty>* operator&() { return this; }
		const TAnyPointer<_Ty>* operator&() const { return this; }
	};

	template <typename _Ty>
	class TCommonConstPointerInterface {
	public:
		virtual ~TCommonConstPointerInterface() {}
		virtual const _Ty& operator*() const = 0;
		virtual const _Ty* operator->() const = 0;
	};

	template <typename _Ty, typename _TConstPointer1>
	class TCommonizedConstPointer : public TCommonConstPointerInterface<_Ty> {
	public:
		TCommonizedConstPointer(const _TConstPointer1& const_pointer) : m_const_pointer(const_pointer) {}
		virtual ~TCommonizedConstPointer() {}

		const _Ty& operator*() const {
			return (*m_const_pointer);
		}
		const _Ty* operator->() const {
			return m_const_pointer.operator->();
		}

		_TConstPointer1 m_const_pointer;
	};

	template <typename _Ty>
	class TXScopeAnyConstPointer {
	public:
		TXScopeAnyConstPointer(const TXScopeAnyConstPointer& src) : m_any_const_pointer(src.m_any_const_pointer) {}

		template <typename _TPointer1>
		TXScopeAnyConstPointer(const _TPointer1& pointer) : m_any_const_pointer(TCommonizedConstPointer<_Ty, _TPointer1>(pointer)) {}

		const _Ty& operator*() const {
			return (*(*common_pointer_interface_const_ptr()));
		}
		const _Ty* operator->() const {
			return common_pointer_interface_const_ptr()->operator->();
		}

	protected:
		TXScopeAnyConstPointer<_Ty>& operator=(const TXScopeAnyConstPointer<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopeAnyConstPointer<_Ty>* operator&() { return this; }
		const TXScopeAnyConstPointer<_Ty>* operator&() const { return this; }

		const TCommonPointerInterface<_Ty>* common_pointer_interface_const_ptr() const {
			auto retval = reinterpret_cast<const TCommonPointerInterface<_Ty>*>(m_any_const_pointer.storage_address());
			assert(nullptr != retval);
			return retval;
		}

		mse::any m_any_const_pointer;
	};

	template <typename _Ty>
	class TAnyConstPointer : public TXScopeAnyConstPointer<_Ty> {
	public:
		TAnyConstPointer(const TAnyConstPointer& src) : TXScopeAnyConstPointer<_Ty>(src) {}

		template <typename _TPointer1
#ifndef MSE_SCOPEPOINTER_DISABLED
			, class = typename std::enable_if<
			(!std::is_convertible<_TPointer1, TXScopeFixedPointer<_Ty>>::value)
			&& (!std::is_convertible<_TPointer1, TXScopeFixedConstPointer<_Ty>>::value)
			&& (!std::is_same<_TPointer1, TXScopeAnyPointer<_Ty>>::value)
			&& (!std::is_same<_TPointer1, TXScopeAnyConstPointer<_Ty>>::value)
			, void>::type
#endif // !MSE_SCOPEPOINTER_DISABLED
		>
			TAnyConstPointer(const _TPointer1& pointer) : TXScopeAnyConstPointer<_Ty>(pointer) {}

	protected:
		TAnyConstPointer<_Ty>& operator=(const TAnyConstPointer<_Ty>& _Right_cref) = delete;

		TAnyConstPointer<_Ty>* operator&() { return this; }
		const TAnyConstPointer<_Ty>* operator&() const { return this; }
	};


	template<typename _Ty>
	class TPolyPointerID {};

	template<typename _Ty>
	class TXScopePolyPointer {
	public:
		using poly_variant = tdp_pointer_variant<
#if !defined(MSE_SCOPEPOINTER_DISABLED)
			mse::TXScopeFixedPointer<_Ty>,
#endif // !defined(MSE_SCOPEPOINTER_DISABLED)
#if !defined(MSE_REGISTEREDPOINTER_DISABLED)
			mse::TRegisteredPointer<_Ty>,
			mse::TRelaxedRegisteredPointer<_Ty>,
#endif // !defined(MSE_REGISTEREDPOINTER_DISABLED)
#if !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
			mse::TRefCountingPointer<_Ty>,
#endif // !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
#if !defined(MSE_MSTDVECTOR_DISABLED)
			typename mse::mstd::vector<_Ty>::iterator,
#endif // !defined(MSE_MSTDVECTOR_DISABLED)
			typename mse::msevector<_Ty>::iterator,
			typename mse::msevector<_Ty>::ipointer,
			typename mse::msevector<_Ty>::ss_iterator_type,
			mse::TAsyncSharedReadWritePointer<_Ty>,
			mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWritePointer<_Ty>,
			std::shared_ptr<_Ty>,
			mse::TXScopeAnyPointer<_Ty>,
			mse::TAnyPointer<_Ty>,

			mse::TPointer<_Ty, TPolyPointerID<const _Ty>>
		>;

		TXScopePolyPointer(const TXScopePolyPointer<_Ty>& p) : m_pointer(p.m_pointer) {}

#if !defined(MSE_SCOPEPOINTER_DISABLED)
		TXScopePolyPointer(const mse::TXScopeFixedPointer<_Ty>& p) { m_pointer.template set<mse::TXScopeFixedPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyPointer(const mse::TXScopeFixedPointer<_Ty2>& p) { m_pointer.template set<mse::TXScopeFixedPointer<_Ty>>(p); }
#endif // !defined(MSE_SCOPEPOINTER_DISABLED)
#if !defined(MSE_REGISTEREDPOINTER_DISABLED)
		TXScopePolyPointer(const mse::TRegisteredPointer<_Ty>& p) { m_pointer.template set<mse::TRegisteredPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<
			std::is_convertible<TRegisteredObj<_Ty2> *, TRegisteredObj<_Ty> *>::value || std::is_same<const _Ty2, _Ty>::value
			, void>::type>
		TXScopePolyPointer(const mse::TRegisteredPointer<_Ty2>& p) { m_pointer.template set<mse::TRegisteredPointer<_Ty>>(p); }

		TXScopePolyPointer(const mse::TRelaxedRegisteredPointer<_Ty>& p) { m_pointer.template set<mse::TRelaxedRegisteredPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyPointer(const mse::TRelaxedRegisteredPointer<_Ty2>& p) { m_pointer.template set<mse::TRelaxedRegisteredPointer<_Ty>>(p); }
#endif // !defined(MSE_REGISTEREDPOINTER_DISABLED)
#if !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
		TXScopePolyPointer(const mse::TRefCountingPointer<_Ty>& p) { m_pointer.template set<mse::TRefCountingPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyPointer(const mse::TRefCountingPointer<_Ty2>& p) { m_pointer.template set<mse::TRefCountingPointer<_Ty>>(p); }
#endif // !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
#if !defined(MSE_MSTDVECTOR_DISABLED)
		TXScopePolyPointer(const typename mse::mstd::vector<_Ty>::iterator& p) { m_pointer.template set<typename mse::mstd::vector<_Ty>::iterator>(p); }
#endif // !defined(MSE_MSTDVECTOR_DISABLED)
		TXScopePolyPointer(const typename mse::msevector<_Ty>::iterator& p) { m_pointer.template set<typename mse::msevector<_Ty>::iterator>(p); }
		TXScopePolyPointer(const typename mse::msevector<_Ty>::ipointer& p) { m_pointer.template set<typename mse::msevector<_Ty>::ipointer>(p); }
		TXScopePolyPointer(const typename mse::msevector<_Ty>::ss_iterator_type& p) { m_pointer.template set<typename mse::msevector<_Ty>::ss_iterator_type>(p); }
		TXScopePolyPointer(const mse::TAsyncSharedReadWritePointer<_Ty>& p) { m_pointer.template set<mse::TAsyncSharedReadWritePointer<_Ty>>(p); }
		TXScopePolyPointer(const mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWritePointer<_Ty>& p) { m_pointer.template set<mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWritePointer<_Ty>>(p); }
		TXScopePolyPointer(const std::shared_ptr<_Ty>& p) { m_pointer.template set<std::shared_ptr<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyPointer(const std::shared_ptr<_Ty2>& p) { m_pointer.template set<std::shared_ptr<_Ty>>(p); }
		TXScopePolyPointer(const mse::TXScopeAnyPointer<_Ty>& p) { m_pointer.template set<mse::TXScopeAnyPointer<_Ty>>(p); }
		TXScopePolyPointer(const mse::TAnyPointer<_Ty>& p) { m_pointer.template set<mse::TAnyPointer<_Ty>>(p); }

		TXScopePolyPointer(_Ty* p) { m_pointer.template set<mse::TPointer<_Ty, TPolyPointerID<const _Ty>>>(p); }

		_Ty& operator*() const {
			return *(reinterpret_cast<_Ty*>(m_pointer.arrow_operator()));
		}
		_Ty* operator->() const {
			return reinterpret_cast<_Ty*>(m_pointer.arrow_operator());
		}

	private:
		TXScopePolyPointer<_Ty>& operator=(const TXScopePolyPointer<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopePolyPointer<_Ty>* operator&() { return this; }
		const TXScopePolyPointer<_Ty>* operator&() const { return this; }

		poly_variant m_pointer;
	};

	template<typename _Ty>
	class TPolyPointer : public TXScopePolyPointer<_Ty> {
	public:

		TPolyPointer(const TPolyPointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}

#if !defined(MSE_REGISTEREDPOINTER_DISABLED)
		TPolyPointer(const mse::TRegisteredPointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<
			std::is_convertible<TRegisteredObj<_Ty2> *, TRegisteredObj<_Ty> *>::value || std::is_same<const _Ty2, _Ty>::value
			, void>::type>
			TPolyPointer(const mse::TRegisteredPointer<_Ty2>& p) : TXScopePolyPointer<_Ty>(p) {}

		TPolyPointer(const mse::TRelaxedRegisteredPointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyPointer(const mse::TRelaxedRegisteredPointer<_Ty2>& p) : TXScopePolyPointer<_Ty>(p) {}
#endif // !defined(MSE_REGISTEREDPOINTER_DISABLED)
#if !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
		TPolyPointer(const mse::TRefCountingPointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyPointer(const mse::TRefCountingPointer<_Ty2>& p) : TXScopePolyPointer<_Ty>(p) {}
#endif // !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
#if !defined(MSE_MSTDVECTOR_DISABLED)
		TPolyPointer(const typename mse::mstd::vector<_Ty>::iterator& p) : TXScopePolyPointer<_Ty>(p) {}
#endif // !defined(MSE_MSTDVECTOR_DISABLED)
		TPolyPointer(const typename mse::msevector<_Ty>::iterator& p) : TXScopePolyPointer<_Ty>(p) {}
		TPolyPointer(const typename mse::msevector<_Ty>::ipointer& p) : TXScopePolyPointer<_Ty>(p) {}
		TPolyPointer(const typename mse::msevector<_Ty>::ss_iterator_type& p) : TXScopePolyPointer<_Ty>(p) {}
		TPolyPointer(const mse::TAsyncSharedReadWritePointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}
		TPolyPointer(const mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWritePointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}
		TPolyPointer(const std::shared_ptr<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyPointer(const std::shared_ptr<_Ty2>& p) : TXScopePolyPointer<_Ty>(p) {}
		//TPolyPointer(const mse::TXScopeAnyPointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}
		TPolyPointer(const mse::TAnyPointer<_Ty>& p) : TXScopePolyPointer<_Ty>(p) {}

		TPolyPointer(_Ty* p) : TXScopePolyPointer<_Ty>(p) {}

	private:
		TPolyPointer<_Ty>& operator=(const TPolyPointer<_Ty>& _Right_cref) = delete;
		//void* operator new(size_t size) { return ::operator new(size); }

		TPolyPointer<_Ty>* operator&() { return this; }
		const TPolyPointer<_Ty>* operator&() const { return this; }
	};

	template<typename _Ty>
	class TXScopePolyConstPointer {
	public:
		using poly_variant = tdp_pointer_variant<
#if !defined(MSE_SCOPEPOINTER_DISABLED)
			mse::TXScopeFixedConstPointer<_Ty>,
#endif // !defined(MSE_SCOPEPOINTER_DISABLED)
#if !defined(MSE_REGISTEREDPOINTER_DISABLED)
			mse::TRegisteredConstPointer<_Ty>,
			mse::TRelaxedRegisteredConstPointer<_Ty>,
#endif // !defined(MSE_REGISTEREDPOINTER_DISABLED)
#if !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
			mse::TRefCountingConstPointer<_Ty>,
#endif // !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
#if !defined(MSE_MSTDVECTOR_DISABLED)
			typename mse::mstd::vector<_Ty>::const_iterator,
#endif // !defined(MSE_MSTDVECTOR_DISABLED)
			typename mse::msevector<_Ty>::const_iterator,
			typename mse::msevector<_Ty>::cipointer,
			typename mse::msevector<_Ty>::ss_const_iterator_type,
			mse::TAsyncSharedReadWriteConstPointer<_Ty>,
			mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWriteConstPointer<_Ty>,
			std::shared_ptr<const _Ty>,
			mse::TXScopeAnyConstPointer<_Ty>,
			mse::TAnyConstPointer<_Ty>,

			mse::TPointer<const _Ty, TPolyPointerID<const _Ty>>,
			mse::TPolyPointer<_Ty>
		>;

		TXScopePolyConstPointer(const TXScopePolyConstPointer<_Ty>& p) : m_pointer(p.m_pointer) {}
		TXScopePolyConstPointer(const mse::TXScopePolyPointer<_Ty>& p) { m_pointer.template set<mse::TXScopePolyPointer<_Ty>>(p); }

#if !defined(MSE_SCOPEPOINTER_DISABLED)
		TXScopePolyConstPointer(const mse::TXScopeFixedConstPointer<_Ty>& p) { m_pointer.template set<mse::TXScopeFixedConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TXScopeFixedConstPointer<_Ty2>& p) { m_pointer.template set<mse::TXScopeFixedConstPointer<_Ty>>(p); }

		TXScopePolyConstPointer(const mse::TXScopeFixedPointer<_Ty>& p) { m_pointer.template set<mse::TXScopeFixedConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TXScopeFixedPointer<_Ty2>& p) { m_pointer.template set<mse::TXScopeFixedConstPointer<_Ty>>(p); }
#endif // !defined(MSE_SCOPEPOINTER_DISABLED)
#if !defined(MSE_REGISTEREDPOINTER_DISABLED)
		TXScopePolyConstPointer(const mse::TRegisteredConstPointer<_Ty>& p) { m_pointer.template set<mse::TRegisteredConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<TRegisteredObj<_Ty2> *, TRegisteredObj<_Ty> *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TRegisteredConstPointer<_Ty2>& p) { m_pointer.template set<mse::TRegisteredConstPointer<_Ty>>(p); }

		TXScopePolyConstPointer(const mse::TRegisteredPointer<_Ty>& p) { m_pointer.template set<mse::TRegisteredConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<TRegisteredObj<_Ty2> *, TRegisteredObj<_Ty> *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TRegisteredPointer<_Ty2>& p) { m_pointer.template set<mse::TRegisteredConstPointer<_Ty>>(p); }

		TXScopePolyConstPointer(const mse::TRelaxedRegisteredConstPointer<_Ty>& p) { m_pointer.template set<mse::TRelaxedRegisteredConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TRelaxedRegisteredConstPointer<_Ty2>& p) { m_pointer.template set<mse::TRelaxedRegisteredConstPointer<_Ty>>(p); }

		TXScopePolyConstPointer(const mse::TRelaxedRegisteredPointer<_Ty>& p) { m_pointer.template set<mse::TRelaxedRegisteredConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TRelaxedRegisteredPointer<_Ty2>& p) { m_pointer.template set<mse::TRelaxedRegisteredConstPointer<_Ty>>(p); }
#endif // !defined(MSE_REGISTEREDPOINTER_DISABLED)
#if !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
		TXScopePolyConstPointer(const mse::TRefCountingConstPointer<_Ty>& p) { m_pointer.template set<mse::TRefCountingConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TRefCountingConstPointer<_Ty2>& p) { m_pointer.template set<mse::TRefCountingConstPointer<_Ty>>(p); }

		TXScopePolyConstPointer(const mse::TRefCountingPointer<_Ty>& p) { m_pointer.template set<mse::TRefCountingConstPointer<_Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const mse::TRefCountingPointer<_Ty2>& p) { m_pointer.template set<mse::TRefCountingConstPointer<_Ty>>(p); }
#endif // !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
#if !defined(MSE_MSTDVECTOR_DISABLED)
		TXScopePolyConstPointer(const typename mse::mstd::vector<_Ty>::const_iterator& p) { m_pointer.template set<typename mse::mstd::vector<_Ty>::const_iterator>(p); }
		TXScopePolyConstPointer(const typename mse::mstd::vector<_Ty>::iterator& p) { m_pointer.template set<typename mse::mstd::vector<_Ty>::const_iterator>(p); }
#endif // !defined(MSE_MSTDVECTOR_DISABLED)
		TXScopePolyConstPointer(const typename mse::msevector<_Ty>::const_iterator& p) { m_pointer.template set<typename mse::msevector<_Ty>::const_iterator>(p); }
		TXScopePolyConstPointer(const typename mse::msevector<_Ty>::cipointer& p) { m_pointer.template set<typename mse::msevector<_Ty>::cipointer>(p); }
		TXScopePolyConstPointer(const typename mse::msevector<_Ty>::ss_const_iterator_type& p) { m_pointer.template set<typename mse::msevector<_Ty>::ss_const_iterator_type>(p); }
		TXScopePolyConstPointer(const mse::TAsyncSharedReadWriteConstPointer<_Ty>& p) { m_pointer.template set<mse::TAsyncSharedReadWriteConstPointer<_Ty>>(p); }
		TXScopePolyConstPointer(const mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWriteConstPointer<_Ty>& p) { m_pointer.template set<mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWriteConstPointer<_Ty>>(p); }
		TXScopePolyConstPointer(const std::shared_ptr<const _Ty>& p) { m_pointer.template set<std::shared_ptr<const _Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const std::shared_ptr<const _Ty2>& p) { m_pointer.template set<std::shared_ptr<const _Ty>>(p); }
		TXScopePolyConstPointer(const mse::TXScopeAnyConstPointer<_Ty>& p) { m_pointer.template set<mse::TXScopeAnyConstPointer<_Ty>>(p); }
		TXScopePolyConstPointer(const mse::TAnyConstPointer<_Ty>& p) { m_pointer.template set<mse::TAnyConstPointer<_Ty>>(p); }

		TXScopePolyConstPointer(const typename mse::msevector<_Ty>::iterator& p) { m_pointer.template set<typename mse::msevector<_Ty>::const_iterator>(p); }
		TXScopePolyConstPointer(const typename mse::msevector<_Ty>::ipointer& p) { m_pointer.template set<typename mse::msevector<_Ty>::cipointer>(p); }
		TXScopePolyConstPointer(const typename mse::msevector<_Ty>::ss_iterator_type& p) { m_pointer.template set<typename mse::msevector<_Ty>::ss_const_iterator_type>(p); }
		TXScopePolyConstPointer(const mse::TAsyncSharedReadWritePointer<_Ty>& p) { m_pointer.template set<mse::TAsyncSharedReadWriteConstPointer<_Ty>>(p); }
		TXScopePolyConstPointer(const mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWritePointer<_Ty>& p) { m_pointer.template set<mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWriteConstPointer<_Ty>>(p); }
		TXScopePolyConstPointer(const std::shared_ptr<_Ty>& p) { m_pointer.template set<std::shared_ptr<const _Ty>>(p); }
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TXScopePolyConstPointer(const std::shared_ptr<_Ty2>& p) { m_pointer.template set<std::shared_ptr<const _Ty>>(p); }
		TXScopePolyConstPointer(const mse::TXScopeAnyPointer<_Ty>& p) { m_pointer.template set<mse::TXScopeAnyConstPointer<_Ty>>(mse::TXScopeAnyConstPointer<_Ty>(p)); }
		TXScopePolyConstPointer(const mse::TAnyPointer<_Ty>& p) { m_pointer.template set<mse::TAnyConstPointer<_Ty>>(mse::TAnyConstPointer<_Ty>(p)); }

		TXScopePolyConstPointer(const _Ty* p) { m_pointer.template set<mse::TPointer<const _Ty, TPolyPointerID<const _Ty>>>(p); }

		const _Ty& operator*() const {
			return *(reinterpret_cast<const _Ty*>(m_pointer.const_arrow_operator()));
		}
		const _Ty* operator->() const {
			return reinterpret_cast<const _Ty*>(m_pointer.const_arrow_operator());
		}

	private:
		TXScopePolyConstPointer<_Ty>& operator=(const TXScopePolyConstPointer<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopePolyConstPointer<_Ty>* operator&() { return this; }
		const TXScopePolyConstPointer<_Ty>* operator&() const { return this; }

		poly_variant m_pointer;
	};

	template<typename _Ty>
	class TPolyConstPointer : public TXScopePolyConstPointer<_Ty> {
	public:

		TPolyConstPointer(const TPolyConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TPolyPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}

#if !defined(MSE_REGISTEREDPOINTER_DISABLED)
		TPolyConstPointer(const mse::TRegisteredConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<TRegisteredObj<_Ty2> *, TRegisteredObj<_Ty> *>::value, void>::type>
		TPolyConstPointer(const mse::TRegisteredConstPointer<_Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}

		TPolyConstPointer(const mse::TRegisteredPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<TRegisteredObj<_Ty2> *, TRegisteredObj<_Ty> *>::value, void>::type>
		TPolyConstPointer(const mse::TRegisteredPointer<_Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}

		TPolyConstPointer(const mse::TRelaxedRegisteredConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyConstPointer(const mse::TRelaxedRegisteredConstPointer<_Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}

		TPolyConstPointer(const mse::TRelaxedRegisteredPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyConstPointer(const mse::TRelaxedRegisteredPointer<_Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}
#endif // !defined(MSE_REGISTEREDPOINTER_DISABLED)
#if !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
		TPolyConstPointer(const mse::TRefCountingConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyConstPointer(const mse::TRefCountingConstPointer<_Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}

		TPolyConstPointer(const mse::TRefCountingPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyConstPointer(const mse::TRefCountingPointer<_Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}
#endif // !defined(MSE_REFCOUNTINGPOINTER_DISABLED)
#if !defined(MSE_MSTDVECTOR_DISABLED)
		TPolyConstPointer(const typename mse::mstd::vector<_Ty>::const_iterator& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const typename mse::mstd::vector<_Ty>::iterator& p) : TXScopePolyConstPointer<_Ty>(p) {}
#endif // !defined(MSE_MSTDVECTOR_DISABLED)
		TPolyConstPointer(const typename mse::msevector<_Ty>::const_iterator& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const typename mse::msevector<_Ty>::cipointer& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const typename mse::msevector<_Ty>::ss_const_iterator_type& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TAsyncSharedReadWriteConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWriteConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const std::shared_ptr<const _Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyConstPointer(const std::shared_ptr<const _Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TXScopeAnyConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TAnyConstPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}

		TPolyConstPointer(const typename mse::msevector<_Ty>::iterator& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const typename mse::msevector<_Ty>::ipointer& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const typename mse::msevector<_Ty>::ss_iterator_type& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TAsyncSharedReadWritePointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TAsyncSharedObjectThatYouAreSureHasNoUnprotectedMutablesReadWritePointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const std::shared_ptr<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TPolyConstPointer(const std::shared_ptr<_Ty2>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TXScopeAnyPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}
		TPolyConstPointer(const mse::TAnyPointer<_Ty>& p) : TXScopePolyConstPointer<_Ty>(p) {}

		TPolyConstPointer(const _Ty* p) : TXScopePolyConstPointer<_Ty>(p) {}

	private:
		TPolyConstPointer<_Ty>& operator=(const TPolyConstPointer<_Ty>& _Right_cref) = delete;
		//void* operator new(size_t size) { return ::operator new(size); }

		TPolyConstPointer<_Ty>* operator&() { return this; }
		const TPolyConstPointer<_Ty>* operator&() const { return this; }
	};


	template <typename _Ty>
	class TCommonRandomAccessIteratorInterface {
	public:
		virtual ~TCommonRandomAccessIteratorInterface() {}
		virtual _Ty& operator*() const = 0;
		virtual _Ty* operator->() const = 0;
		typedef typename mse::mstd::array<_Ty, 0>::reference reference_t;
		typedef typename mse::mstd::array<_Ty, 0>::difference_type difference_t;
		virtual reference_t operator[](difference_t _Off) const = 0;
		virtual void operator +=(difference_t x) = 0;
		virtual void operator -=(difference_t x) { operator +=(-x); }
		virtual void operator ++() { operator +=(1); }
		virtual void operator ++(int) { operator +=(1); }
		virtual void operator --() { operator -=(1); }
		virtual void operator --(int) { operator -=(1); }
	};

	template <typename _Ty, typename _TRandomAccessIterator1>
	class TCommonizedRandomAccessIterator : public TCommonRandomAccessIteratorInterface<_Ty> {
	public:
		TCommonizedRandomAccessIterator(const _TRandomAccessIterator1& random_access_iterator) : m_random_access_iterator(random_access_iterator) {}
		virtual ~TCommonizedRandomAccessIterator() {}

		_Ty& operator*() const {
			return (*m_random_access_iterator);
		}
		_Ty* operator->() const {
			return m_random_access_iterator.operator->();
		}
		typename TCommonRandomAccessIteratorInterface<_Ty>::reference_t operator[](typename TCommonRandomAccessIteratorInterface<_Ty>::difference_t _Off) const {
			return m_random_access_iterator[_Off];
		}
		void operator +=(typename TCommonRandomAccessIteratorInterface<_Ty>::difference_t x) { m_random_access_iterator += x; };

		_TRandomAccessIterator1 m_random_access_iterator;
	};

	template <typename _Ty>
	class TAnyRandomAccessIterator;

	template <typename _Ty>
	class TXScopeAnyRandomAccessIterator {
	public:
		TXScopeAnyRandomAccessIterator(const TXScopeAnyRandomAccessIterator& src) : m_any_random_access_iterator(src.m_any_random_access_iterator) {}

		template <typename _TRandomAccessIterator1, class = typename std::enable_if<!std::is_convertible<_TRandomAccessIterator1, TXScopeAnyRandomAccessIterator>::value, void>::type>
		TXScopeAnyRandomAccessIterator(const _TRandomAccessIterator1& random_access_iterator) : m_any_random_access_iterator(TCommonizedRandomAccessIterator<_Ty, _TRandomAccessIterator1>(random_access_iterator)) {}

		_Ty& operator*() const {
			return (*(*common_random_access_iterator_interface_ptr()));
		}
		_Ty* operator->() const {
			return common_random_access_iterator_interface_ptr()->operator->();
		}
		typedef typename TCommonRandomAccessIteratorInterface<_Ty>::reference_t reference_t;
		typedef typename TCommonRandomAccessIteratorInterface<_Ty>::difference_t difference_t;
		reference_t operator[](difference_t _Off) const {
			return common_random_access_iterator_interface_ptr()->operator[](_Off);
		}
		void operator +=(difference_t x) { common_random_access_iterator_interface_ptr()->operator+=(x); };
		void operator -=(difference_t x) { operator +=(-x); }
		void operator ++() { operator +=(1); }
		void operator ++(int) { operator +=(1); }
		void operator --() { operator -=(1); }
		void operator --(int) { operator -=(1); }

	private:
		TXScopeAnyRandomAccessIterator<_Ty>& operator=(const TXScopeAnyRandomAccessIterator<_Ty>& _Right_cref) = default;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopeAnyRandomAccessIterator<_Ty>* operator&() { return this; }
		const TXScopeAnyRandomAccessIterator<_Ty>* operator&() const { return this; }

		TCommonRandomAccessIteratorInterface<_Ty>* common_random_access_iterator_interface_ptr() {
			auto retval = reinterpret_cast<TCommonRandomAccessIteratorInterface<_Ty>*>(m_any_random_access_iterator.storage_address());
			assert(nullptr != retval);
			return retval;
		}
		const TCommonRandomAccessIteratorInterface<_Ty>* common_random_access_iterator_interface_ptr() const {
			auto retval = reinterpret_cast<const TCommonRandomAccessIteratorInterface<_Ty>*>(m_any_random_access_iterator.storage_address());
			assert(nullptr != retval);
			return retval;
		}

		mse::any m_any_random_access_iterator;

		friend class TAnyRandomAccessIterator<_Ty>;
	};

	template <typename _Ty>
	class TAnyRandomAccessIterator : public TXScopeAnyRandomAccessIterator<_Ty> {
	public:
		TAnyRandomAccessIterator(const TAnyRandomAccessIterator& src) : TXScopeAnyRandomAccessIterator<_Ty>(static_cast<TXScopeAnyRandomAccessIterator<_Ty>>(src)) {}

		template <typename _TRandomAccessIterator1, class = typename std::enable_if<!std::is_convertible<_TRandomAccessIterator1, TAnyRandomAccessIterator>::value, void>::type>
		TAnyRandomAccessIterator(const _TRandomAccessIterator1& random_access_iterator) : TXScopeAnyRandomAccessIterator<_Ty>(random_access_iterator) {}

		TAnyRandomAccessIterator<_Ty>& operator=(const TAnyRandomAccessIterator<_Ty>& _Right_cref) {
			TXScopeAnyRandomAccessIterator<_Ty>::operator=(_Right_cref);
			return (*this);
		}
		//void* operator new(size_t size) { return ::operator new(size); }

	private:
		TAnyRandomAccessIterator<_Ty>* operator&() { return this; }
		const TAnyRandomAccessIterator<_Ty>* operator&() const { return this; }
	};

	template <typename _Ty>
	class TAnyConstRandomAccessIterator;

	template <typename _Ty>
	class TCommonConstRandomAccessIteratorInterface {
	public:
		virtual ~TCommonConstRandomAccessIteratorInterface() {}
		virtual const _Ty& operator*() const = 0;
		virtual const _Ty* operator->() const = 0;
		typedef typename mse::mstd::array<_Ty, 0>::const_reference const_reference_t;
		typedef typename mse::mstd::array<_Ty, 0>::difference_type difference_t;
		virtual const_reference_t operator[](difference_t _Off) const = 0;
		virtual void operator +=(difference_t x) = 0;
		virtual void operator -=(difference_t x) { operator +=(-x); }
		virtual void operator ++() { operator +=(1); }
		virtual void operator ++(int) { operator +=(1); }
		virtual void operator --() { operator -=(1); }
		virtual void operator --(int) { operator -=(1); }
	};

	template <typename _Ty, typename _TConstRandomAccessIterator1>
	class TCommonizedConstRandomAccessIterator : public TCommonConstRandomAccessIteratorInterface<_Ty> {
	public:
		TCommonizedConstRandomAccessIterator(const _TConstRandomAccessIterator1& const_random_access_iterator) : m_const_random_access_iterator(const_random_access_iterator) {}
		virtual ~TCommonizedConstRandomAccessIterator() {}

		const _Ty& operator*() const {
			return (*m_const_random_access_iterator);
		}
		const _Ty* operator->() const {
			return m_const_random_access_iterator.operator->();
		}
		typename TCommonConstRandomAccessIteratorInterface<_Ty>::const_reference_t operator[](typename TCommonConstRandomAccessIteratorInterface<_Ty>::difference_t _Off) const {
			return m_const_random_access_iterator[_Off];
		}
		void operator +=(typename TCommonConstRandomAccessIteratorInterface<_Ty>::difference_t x) { m_const_random_access_iterator += x; };

		_TConstRandomAccessIterator1 m_const_random_access_iterator;
	};

	template <typename _Ty>
	class TAnyConstRandomAccessIterator;

	template <typename _Ty>
	class TXScopeAnyConstRandomAccessIterator {
	public:
		TXScopeAnyConstRandomAccessIterator(const TXScopeAnyConstRandomAccessIterator& src) : m_any_const_random_access_iterator(src.m_any_const_random_access_iterator) {}

		template <typename _TConstRandomAccessIterator1, class = typename std::enable_if<!std::is_convertible<_TConstRandomAccessIterator1, TXScopeAnyConstRandomAccessIterator>::value, void>::type>
		TXScopeAnyConstRandomAccessIterator(const _TConstRandomAccessIterator1& const_random_access_iterator) : m_any_const_random_access_iterator(TCommonizedConstRandomAccessIterator<const _Ty, _TConstRandomAccessIterator1>(const_random_access_iterator)) {}

		const _Ty& operator*() const {
			return (*(*common_const_random_access_iterator_interface_ptr()));
		}
		const _Ty* operator->() const {
			return common_const_random_access_iterator_interface_ptr()->operator->();
		}
		typedef typename TCommonConstRandomAccessIteratorInterface<_Ty>::const_reference_t const_reference_t;
		typedef typename TCommonConstRandomAccessIteratorInterface<_Ty>::difference_t difference_t;
		const_reference_t operator[](difference_t _Off) const {
			return common_const_random_access_iterator_interface_ptr()->operator[](_Off);
		}
		void operator +=(difference_t x) { common_const_random_access_iterator_interface_ptr()->operator+=(x); };
		void operator -=(difference_t x) { operator +=(-x); }
		void operator ++() { operator +=(1); }
		void operator ++(int) { operator +=(1); }
		void operator --() { operator -=(1); }
		void operator --(int) { operator -=(1); }

	private:
		TXScopeAnyConstRandomAccessIterator<_Ty>& operator=(const TXScopeAnyConstRandomAccessIterator<_Ty>& _Right_cref) = default;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopeAnyConstRandomAccessIterator<_Ty>* operator&() { return this; }
		const TXScopeAnyConstRandomAccessIterator<_Ty>* operator&() const { return this; }

		TCommonConstRandomAccessIteratorInterface<_Ty>* common_const_random_access_iterator_interface_ptr() {
			auto retval = reinterpret_cast<TCommonConstRandomAccessIteratorInterface<_Ty>*>(m_any_const_random_access_iterator.storage_address());
			assert(nullptr != retval);
			return retval;
		}
		const TCommonConstRandomAccessIteratorInterface<_Ty>* common_const_random_access_iterator_interface_ptr() const {
			auto retval = reinterpret_cast<const TCommonConstRandomAccessIteratorInterface<_Ty>*>(m_any_const_random_access_iterator.storage_address());
			assert(nullptr != retval);
			return retval;
		}

		mse::any m_any_const_random_access_iterator;

		friend class TAnyConstRandomAccessIterator<_Ty>;
	};

	template <typename _Ty>
	class TAnyConstRandomAccessIterator : public TXScopeAnyConstRandomAccessIterator<_Ty> {
	public:
		TAnyConstRandomAccessIterator(const TAnyConstRandomAccessIterator& src) : TXScopeAnyConstRandomAccessIterator<_Ty>(static_cast<TXScopeAnyConstRandomAccessIterator<_Ty>>(src)) {}

		template <typename _TConstRandomAccessIterator1, class = typename std::enable_if<!std::is_convertible<_TConstRandomAccessIterator1, TAnyConstRandomAccessIterator>::value, void>::type>
		TAnyConstRandomAccessIterator(const _TConstRandomAccessIterator1& const_random_access_iterator) : TXScopeAnyConstRandomAccessIterator<_Ty>(const_random_access_iterator) {}

		TAnyConstRandomAccessIterator<_Ty>& operator=(const TAnyConstRandomAccessIterator<_Ty>& _Right_cref) {
			TXScopeAnyConstRandomAccessIterator<_Ty>::operator=(_Right_cref);
			return (*this);
		}
		//void* operator new(size_t size) { return ::operator new(size); }

	private:
		TAnyConstRandomAccessIterator<_Ty>* operator&() { return this; }
		const TAnyConstRandomAccessIterator<_Ty>* operator&() const { return this; }
	};

	//template<typename _Ty> using TAnyArrayIterator = TAnyRandomAccessIterator<_Ty>;
	//template<typename _Ty> using TAnyConstArrayIterator = TAnyConstRandomAccessIterator<_Ty>;

	template <typename _Ty>
	class TRandomAccessSection;
	template <typename _Ty>
	class TConstRandomAccessSection;

	template <typename _Ty>
	class TXScopeRandomAccessSection {
	public:
		typedef typename TXScopeAnyRandomAccessIterator<_Ty>::reference_t reference_t;
		typedef typename mse::mstd::array<_Ty, 0>::size_type size_type;
		typedef typename TXScopeAnyRandomAccessIterator<_Ty>::difference_t difference_t;

		TXScopeRandomAccessSection(const TXScopeAnyRandomAccessIterator<_Ty>& start_iter, size_type count) : m_start_iter(start_iter), m_count(count) {}
		TXScopeRandomAccessSection(const TXScopeRandomAccessSection& src) = default;
		TXScopeRandomAccessSection(const TRandomAccessSection<_Ty>& src) : m_start_iter(src.begin()), m_count(src.size()) {}

		reference_t operator[](size_type _P) const {
			if (m_count <= _P) { MSE_THROW(msearray_range_error("out of bounds index - reference_t operator[](size_type _P) - TXScopeRandomAccessSection")); }
			return m_start_iter[difference_t(_P)];
		}
		size_type size() const {
			return m_count;
		}
		TXScopeAnyRandomAccessIterator<_Ty> begin() const { return m_start_iter; }
		TXScopeAnyConstRandomAccessIterator<_Ty> cbegin() const { return m_start_iter; }

	private:
		TXScopeRandomAccessSection<_Ty>& operator=(const TXScopeRandomAccessSection<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopeRandomAccessSection<_Ty>* operator&() { return this; }
		const TXScopeRandomAccessSection<_Ty>* operator&() const { return this; }

		TXScopeAnyRandomAccessIterator<_Ty> m_start_iter;
		const size_type m_count = 0;
	};

	template <typename _Ty>
	class TXScopeConstRandomAccessSection {
	public:
		typedef typename TXScopeAnyConstRandomAccessIterator<_Ty>::const_reference_t const_reference_t;
		typedef typename mse::mstd::array<_Ty, 0>::size_type size_type;
		typedef typename TXScopeAnyConstRandomAccessIterator<_Ty>::difference_t difference_t;

		TXScopeConstRandomAccessSection(const TXScopeAnyConstRandomAccessIterator<_Ty>& start_const_iter, size_type count) : m_start_const_iter(start_const_iter), m_count(count) {}
		TXScopeConstRandomAccessSection(const TXScopeConstRandomAccessSection& src) = default;
		TXScopeConstRandomAccessSection(const TConstRandomAccessSection<_Ty>& src) : m_start_const_iter(src.cbegin()), m_count(src.size()) {}

		const_reference_t operator[](size_type _P) const {
			if (m_count <= _P) { MSE_THROW(msearray_range_error("out of bounds index - const_reference_t operator[](size_type _P) - TXScopeConstRandomAccessSection")); }
			return m_start_const_iter[difference_t(_P)];
		}
		size_type size() const {
			return m_count;
		}
		TXScopeAnyConstRandomAccessIterator<_Ty> cbegin() const { return m_start_const_iter; }

	private:
		TXScopeConstRandomAccessSection<_Ty>& operator=(const TXScopeConstRandomAccessSection<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		TXScopeConstRandomAccessSection<_Ty>* operator&() { return this; }
		const TXScopeConstRandomAccessSection<_Ty>* operator&() const { return this; }

		const TXScopeAnyConstRandomAccessIterator<_Ty> m_start_const_iter;
		const size_type m_count = 0;
	};

	//template<typename _Ty> using TArraySection = TXScopeRandomAccessSection<_Ty>;
	//template<typename _Ty> using TConstArraySection = TXScopeConstRandomAccessSection<_Ty>;

	template <typename _Ty>
	class TRandomAccessSection {
	public:
		typedef typename TAnyRandomAccessIterator<_Ty>::reference_t reference_t;
		typedef typename mse::mstd::array<_Ty, 0>::size_type size_type;
		typedef typename TAnyRandomAccessIterator<_Ty>::difference_t difference_t;

		TRandomAccessSection(const TAnyRandomAccessIterator<_Ty>& start_iter, size_type count) : m_start_iter(start_iter), m_count(count) {}
		TRandomAccessSection(const TRandomAccessSection& src) = default;

		reference_t operator[](size_type _P) const {
			if (m_count <= _P) { MSE_THROW(msearray_range_error("out of bounds index - reference_t operator[](size_type _P) - TRandomAccessSection")); }
			return m_start_iter[difference_t(_P)];
		}
		size_type size() const {
			return m_count;
		}
		TAnyRandomAccessIterator<_Ty> begin() const { return m_start_iter; }
		TAnyConstRandomAccessIterator<_Ty> cbegin() const { return m_start_iter; }

		const TAnyRandomAccessIterator<_Ty> m_start_iter;
		const size_type m_count = 0;
	};

	template <typename _Ty>
	class TConstRandomAccessSection {
	public:
		typedef typename TAnyConstRandomAccessIterator<_Ty>::const_reference_t const_reference_t;
		typedef typename mse::mstd::array<_Ty, 0>::size_type size_type;
		typedef typename TAnyConstRandomAccessIterator<_Ty>::difference_t difference_t;

		TConstRandomAccessSection(const TAnyConstRandomAccessIterator<_Ty>& start_const_iter, size_type count) : m_start_const_iter(start_const_iter), m_count(count) {}
		TConstRandomAccessSection(const TConstRandomAccessSection& src) = default;

		const_reference_t operator[](size_type _P) const {
			if (m_count <= _P) { MSE_THROW(msearray_range_error("out of bounds index - const_reference_t operator[](size_type _P) - TConstRandomAccessSection")); }
			return m_start_const_iter[difference_t(_P)];
		}
		size_type size() const {
			return m_count;
		}
		TAnyConstRandomAccessIterator<_Ty> cbegin() const { return m_start_const_iter; }

		const TAnyConstRandomAccessIterator<_Ty> m_start_const_iter;
		const size_type m_count = 0;
	};

	//template<typename _Ty> using TArraySection = TRandomAccessSection<_Ty>;
	//template<typename _Ty> using TConstArraySection = TConstRandomAccessSection<_Ty>;


	/* shorter aliases */
	template<typename _Ty> using pp = TPolyPointer<_Ty>;
	template<typename _Ty> using pcp = TPolyConstPointer<_Ty>;
	template<typename _Ty> using anyp = TAnyPointer<_Ty>;
	template<typename _Ty> using anycp = TAnyConstPointer<_Ty>;


	/* Deprecated poly pointers. */
	template<typename _Ty> class TRefCountingOrXScopeFixedConstPointer;

	template<typename _Ty>
	class TRefCountingOrXScopeFixedPointer : public TXScopePolyPointer<_Ty> {
	public:
		TRefCountingOrXScopeFixedPointer(const TRefCountingOrXScopeFixedPointer& src_cref) : TXScopePolyPointer<_Ty>(src_cref) {}
		TRefCountingOrXScopeFixedPointer(const TRefCountingPointer<_Ty>& src_cref) : TXScopePolyPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TRefCountingOrXScopeFixedPointer(const TRefCountingPointer<_Ty2>& src_cref) : TXScopePolyPointer<_Ty>(TRefCountingPointer<_Ty>(src_cref)) {}
		TRefCountingOrXScopeFixedPointer(const TXScopeFixedPointer<_Ty>& src_cref) : TXScopePolyPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TRefCountingOrXScopeFixedPointer(const TXScopeFixedPointer<_Ty2>& src_cref) : TXScopePolyPointer<_Ty>(TXScopeFixedPointer<_Ty>(src_cref)) {}
		virtual ~TRefCountingOrXScopeFixedPointer() {}
		/* This native pointer cast operator is just for compatibility with existing/legacy code and ideally should never be used. */
		explicit operator _Ty*() const { return std::addressof((*this).operator*()); }

#ifndef MSE_SCOPEPOINTER_DISABLED
	protected:
		TRefCountingOrXScopeFixedPointer(_Ty* ptr) : TXScopePolyPointer<_Ty>(ptr) {}
#endif // !MSE_SCOPEPOINTER_DISABLED
	private:
		TRefCountingOrXScopeFixedPointer<_Ty>& operator=(const TRefCountingOrXScopeFixedPointer<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		//TRefCountingOrXScopeFixedPointer<_Ty>* operator&() { return this; }
		//const TRefCountingOrXScopeFixedPointer<_Ty>* operator&() const { return this; }

		friend class TRefCountingOrXScopeFixedConstPointer<_Ty>;
	};

	template<typename _Ty>
	class TRefCountingOrXScopeFixedConstPointer : public TXScopePolyConstPointer<_Ty> {
	public:
		TRefCountingOrXScopeFixedConstPointer(const TRefCountingOrXScopeFixedConstPointer& src_cref) : TXScopePolyConstPointer<_Ty>(src_cref) {}
		TRefCountingOrXScopeFixedConstPointer(const TRefCountingOrXScopeFixedPointer<_Ty>& src_cref) : TXScopePolyConstPointer<_Ty>(src_cref) {}
		TRefCountingOrXScopeFixedConstPointer(const TRefCountingConstPointer<_Ty>& src_cref) : TXScopePolyConstPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TRefCountingOrXScopeFixedConstPointer(const TRefCountingConstPointer<_Ty2>& src_cref) : TXScopePolyConstPointer<_Ty>(TRefCountingConstPointer<_Ty>(src_cref)) {}
		TRefCountingOrXScopeFixedConstPointer(const TRefCountingPointer<_Ty>& src_cref) : TXScopePolyConstPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TRefCountingOrXScopeFixedConstPointer(const TRefCountingPointer<_Ty2>& src_cref) : TXScopePolyConstPointer<_Ty>(TRefCountingPointer<_Ty>(src_cref)) {}
		TRefCountingOrXScopeFixedConstPointer(const TXScopeFixedConstPointer<_Ty>& src_cref) : TXScopePolyConstPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TRefCountingOrXScopeFixedConstPointer(const TXScopeFixedConstPointer<_Ty2>& src_cref) : TXScopePolyConstPointer<_Ty>(TXScopeFixedConstPointer<_Ty>(src_cref)) {}
		TRefCountingOrXScopeFixedConstPointer(const TXScopeFixedPointer<_Ty>& src_cref) : TXScopePolyConstPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TRefCountingOrXScopeFixedConstPointer(const TXScopeFixedPointer<_Ty2>& src_cref) : TXScopePolyConstPointer<_Ty>(TXScopeFixedPointer<_Ty>(src_cref)) {}
		virtual ~TRefCountingOrXScopeFixedConstPointer() {}
		/* This native pointer cast operator is just for compatibility with existing/legacy code and ideally should never be used. */
		explicit operator const _Ty*() const { return std::addressof((*this).operator*()); }

#ifndef MSE_SCOPEPOINTER_DISABLED
	protected:
		TRefCountingOrXScopeFixedConstPointer(_Ty* ptr) : TXScopePolyConstPointer<_Ty>(ptr) {}
#endif // !MSE_SCOPEPOINTER_DISABLED
	private:
		TRefCountingOrXScopeFixedConstPointer<_Ty>& operator=(const TRefCountingOrXScopeFixedConstPointer<_Ty>& _Right_cref) = delete;
		void* operator new(size_t size) { return ::operator new(size); }

		//TRefCountingOrXScopeFixedConstPointer<_Ty>* operator&() { return this; }
		//const TRefCountingOrXScopeFixedConstPointer<_Ty>* operator&() const { return this; }
	};


	template<typename _Ty>
	class TRefCountingOrXScopeOrRawFixedPointer : public TRefCountingOrXScopeFixedPointer<_Ty> {
	public:
		MSE_SCOPE_USING(TRefCountingOrXScopeOrRawFixedPointer, TRefCountingOrXScopeFixedPointer<_Ty>);
		TRefCountingOrXScopeOrRawFixedPointer(_Ty* ptr) : TRefCountingOrXScopeFixedPointer<_Ty>(ptr) {}
	};

	template<typename _Ty>
	class TRefCountingOrXScopeOrRawFixedConstPointer : public TRefCountingOrXScopeFixedConstPointer<_Ty> {
	public:
		MSE_SCOPE_USING(TRefCountingOrXScopeOrRawFixedConstPointer, TRefCountingOrXScopeFixedConstPointer<_Ty>);
		TRefCountingOrXScopeOrRawFixedConstPointer(_Ty* ptr) : TRefCountingOrXScopeFixedConstPointer<_Ty>(ptr) {}
	};


	template<typename _Ty> class TSharedOrRawFixedConstPointer;

	template<typename _Ty>
	class TSharedOrRawFixedPointer : public TPolyPointer<_Ty> {
	public:
		TSharedOrRawFixedPointer(const TSharedOrRawFixedPointer& src_cref) : TPolyPointer<_Ty>(src_cref) {}
		TSharedOrRawFixedPointer(const std::shared_ptr<_Ty>& src_cref) : TPolyPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TSharedOrRawFixedPointer(const std::shared_ptr<_Ty2>& src_cref) : TPolyPointer<_Ty>(src_cref) {}
		TSharedOrRawFixedPointer(_Ty* ptr) : TPolyPointer<_Ty>(ptr) {}
		virtual ~TSharedOrRawFixedPointer() {}
		/* This native pointer cast operator is just for compatibility with existing/legacy code and ideally should never be used. */
		explicit operator _Ty*() const { return std::addressof((*this).operator*()); }

	private:
		TSharedOrRawFixedPointer<_Ty>& operator=(const TSharedOrRawFixedPointer<_Ty>& _Right_cref) = delete;

		//TSharedOrRawFixedPointer<_Ty>* operator&() { return this; }
		//const TSharedOrRawFixedPointer<_Ty>* operator&() const { return this; }

		friend class TSharedOrRawFixedConstPointer<_Ty>;
	};

	template<typename _Ty>
	class TSharedOrRawFixedConstPointer : public TPolyConstPointer<_Ty> {
	public:
		TSharedOrRawFixedConstPointer(const TSharedOrRawFixedConstPointer& src_cref) : TPolyConstPointer<_Ty>(src_cref) {}
		TSharedOrRawFixedConstPointer(const TSharedOrRawFixedPointer<_Ty>& src_cref) : TPolyConstPointer<_Ty>(src_cref) {}
		TSharedOrRawFixedConstPointer(const std::shared_ptr<const _Ty>& src_cref) : TPolyConstPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TSharedOrRawFixedConstPointer(const std::shared_ptr<const _Ty2>& src_cref) : TPolyConstPointer<_Ty>(src_cref) {}
		TSharedOrRawFixedConstPointer(const std::shared_ptr<_Ty>& src_cref) : TPolyConstPointer<_Ty>(src_cref) {}
		template<class _Ty2, class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value, void>::type>
		TSharedOrRawFixedConstPointer(const std::shared_ptr<_Ty2>& src_cref) : TStrongFixedConstPointer<_Ty, std::shared_ptr<_Ty>>(src_cref) {}
		TSharedOrRawFixedConstPointer(_Ty* ptr) : TPolyConstPointer<_Ty>(ptr) {}
		virtual ~TSharedOrRawFixedConstPointer() {}
		/* This native pointer cast operator is just for compatibility with existing/legacy code and ideally should never be used. */
		explicit operator const _Ty*() const { return std::addressof((*this).operator*()); }

	private:
		TSharedOrRawFixedConstPointer<_Ty>& operator=(const TSharedOrRawFixedConstPointer<_Ty>& _Right_cref) = delete;

		//TSharedOrRawFixedConstPointer<_Ty>* operator&() { return this; }
		//const TSharedOrRawFixedConstPointer<_Ty>* operator&() const { return this; }
	};



	static void s_poly_test1() {
#ifdef MSE_SELF_TESTS
		{
			class A {
			public:
				A() {}
				A(int x) : b(x) {}
				virtual ~A() {}

				int b = 3;
			};
			class D : public A {
			public:
				D(int x) : A(x) {}
			};
			class B {
			public:
				static int foo1(mse::TXScopePolyPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}
				static int foo2(mse::TXScopePolyConstPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}

				/* Deprecated poly pointers */
				static int foo3(mse::TRefCountingOrXScopeOrRawFixedPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}
				static int foo4(mse::TRefCountingOrXScopeOrRawFixedConstPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}
				static int foo5(mse::TSharedOrRawFixedPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}
				static int foo6(mse::TSharedOrRawFixedConstPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}
				static int foo7(mse::TRefCountingOrXScopeFixedPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}
				static int foo8(mse::TRefCountingOrXScopeFixedConstPointer<A> ptr) {
					int retval = ptr->b;
					return retval;
				}
			protected:
				~B() {}
			};

			/* To demonstrate, first we'll declare some objects such that we can obtain safe pointers to those
			objects. For better or worse, this library provides a bunch of different safe pointers types. */
			mse::TXScopeObj<A> a_scpobj;
			auto a_refcptr = mse::make_refcounting<A>();
			mse::TRegisteredObj<A> a_regobj;
			mse::TRelaxedRegisteredObj<A> a_rlxregobj;

			/* Safe iterators are a type of safe pointer too. */
			mse::mstd::vector<A> a_mstdvec;
			a_mstdvec.resize(1);
			auto a_mstdvec_iter = a_mstdvec.begin();
			mse::msevector<A> a_msevec;
			a_msevec.resize(1);
			auto a_msevec_ipointer = a_msevec.ibegin();
			auto a_msevec_ssiter = a_msevec.ss_begin();

			/* And don't forget the safe async sharing pointers. */
			auto a_access_requester = mse::make_asyncsharedreadwrite<A>();
			auto a_writelock_ptr = a_access_requester.writelock_ptr();
			auto a_stdshared_const_ptr = mse::make_stdsharedimmutable<A>();

			{
				/* All of these safe pointer types happily convert to an mse::TXScopePolyPointer<>. */
				auto res_using_scpptr = B::foo1(&a_scpobj);
				auto res_using_refcptr = B::foo1(a_refcptr);
				auto res_using_regptr = B::foo1(&a_regobj);
				auto res_using_rlxregptr = B::foo1(&a_rlxregobj);
				auto res_using_mstdvec_iter = B::foo1(a_mstdvec_iter);
				auto res_using_msevec_ipointer = B::foo1(a_msevec_ipointer);
				auto res_using_msevec_ssiter = B::foo1(a_msevec_ssiter);
				auto res_using_writelock_ptr = B::foo1(a_writelock_ptr);

				/* Or an mse::TXScopePolyConstPointer<>. */
				auto res_using_scpptr_via_const_poly = B::foo2(&a_scpobj);
				auto res_using_refcptr_via_const_poly = B::foo2(a_refcptr);
				auto res_using_regptr_via_const_poly = B::foo2(&a_regobj);
				auto res_using_rlxregptr_via_const_poly = B::foo2(&a_rlxregobj);
				auto res_using_mstdvec_iter_via_const_poly = B::foo2(a_mstdvec_iter);
				auto res_using_msevec_ipointer_via_const_poly = B::foo2(a_msevec_ipointer);
				auto res_using_msevec_ssiter_via_const_poly = B::foo2(a_msevec_ssiter);
				auto res_using_writelock_ptr_via_const_poly = B::foo2(a_writelock_ptr);
				auto res_using_stdshared_const_ptr_via_const_poly = B::foo2(a_stdshared_const_ptr);

				mse::TXScopePolyPointer<A> a_polyptr(a_refcptr);
				mse::TXScopePolyPointer<A> a_polyptr2(a_polyptr);
				mse::TXScopePolyConstPointer<A> a_polycptr(a_polyptr);
				mse::TXScopePolyConstPointer<A> a_polycptr2(a_polycptr);
			}

			{
				/* Inheritance polymorphism.  */
				auto D_refcfp = mse::make_refcounting<D>(5);
				mse::TXScopeObj<D> d_xscpobj(7);
				D d_obj(11);
				int res11 = B::foo1(D_refcfp);
				int res12 = B::foo1(&d_xscpobj);
				int res13 = B::foo2(D_refcfp);
				int res14 = B::foo2(&d_xscpobj);
			}

			{
				/* Testing the deprecated poly pointers */
				auto A_refcfp = mse::make_refcounting<A>(5);
				mse::TXScopeObj<A> a_xscpobj(7);
				A a_obj(11);
				int res1 = B::foo7(A_refcfp);
				int res2 = B::foo7(&a_xscpobj);
				int res3 = B::foo8(A_refcfp);
				int res4 = B::foo8(&a_xscpobj);

				auto D_refcfp = mse::make_refcounting<D>(5);
				mse::TXScopeObj<D> d_xscpobj(7);
				D d_obj(11);
				int res11 = B::foo7(D_refcfp);
				int res12 = B::foo7(&d_xscpobj);
				int res13 = B::foo8(D_refcfp);
				int res14 = B::foo8(&d_xscpobj);

				int res21 = B::foo3(A_refcfp);
				int res22 = B::foo3(&a_xscpobj);
				int res23 = B::foo3(&a_obj);
				int res24 = B::foo4(A_refcfp);
				int res25 = B::foo4(&a_xscpobj);
				int res26 = B::foo4(&a_obj);

				int res31 = B::foo3(D_refcfp);
				int res32 = B::foo3(&d_xscpobj);
				int res33 = B::foo3(&d_obj);
				int res34 = B::foo4(D_refcfp);
				int res35 = B::foo4(&d_xscpobj);
				int res36 = B::foo4(&d_obj);

				auto A_shp = std::make_shared<A>(5);
				int res41 = B::foo5(A_shp);
				int res42 = B::foo5(&a_obj);
				int res43 = B::foo6(A_shp);
				int res44 = B::foo6(&a_obj);
			}

			{
				/* Just exercising the tdp_variant type. */
				auto A_refcfp = mse::make_refcounting<A>(5);
				mse::TXScopeObj<A> a_xscpobj(7);

				using my_var = tdp_variant<A*, mse::TScopeFixedPointer<A>, mse::TRefCountingFixedPointer<A>>;

				my_var d;

				d.set<mse::TScopeFixedPointer<A>>(&a_xscpobj);
				//std::cout << d.get<mse::TScopeFixedPointer<A>>()->b << std::endl;

				d.set<mse::TRefCountingFixedPointer<A>>(A_refcfp);
				d.get<mse::TRefCountingFixedPointer<A>>()->b = 42;

				my_var e(std::move(d));
				//std::cout << e.get<mse::TRefCountingFixedPointer<A>>()->b << std::endl;

				e.get<mse::TRefCountingFixedPointer<A>>()->b = 43;

				d = e;

				//std::cout << d.get<mse::TRefCountingFixedPointer<A>>()->b << std::endl;
			}
			int q = 3;
		}
#endif // MSE_SELF_TESTS
	}
}

#endif // MSEPOLY_H_