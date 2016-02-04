
// Copyright (c) 2015 Noah Lopez
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//include "msetl.h"
#include "msemsevector.h"
#include "msemstdvector.h"
#include "mseivector.h"
#include "msevector_test.h"
#include "mseregistered.h"
#include "mseregisteredforlegacy.h"
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

/* This block of includes is required for the mse::TRegisteredRefWrapper example */
#include <algorithm>
#include <list>
#include <vector>
#include <iostream>
#include <numeric>
#include <random>
#include <functional>


int main(int argc, char* argv[])
{
	mse::msevector_test msevector_test;
	msevector_test.run_all();

	{
		/* mse::mstd::vector<> is an almost "completely safe" (bounds checked, iterator checked and memory managed)
		implementation of std::vector. Here we'll demonstate the safety of the insert() member function. */

		double a1[3] = { 1.0, 2.0, 3.0 };
		double *d_pointer1 = &(a1[0]);
		double a2[3] = { 4.0, 5.0, 360 };
		double *d_pointer2 = &(a2[0]);
		mse::mstd::vector<double> v1;
		//v1.insert(v1.begin(), d_pointer1, d_pointer2); /* not good */
		/* std::vector supports "naked" pointers as parameters to the insert() member
		function so mse::mstd::vector does also. Unfortunately there is no way to ensure
		that the naked pointer parameters have valid values. */

#ifdef MSVC2010_COMPATIBILE
		mse::mstd::vector<double> v2(a1, a1+3);
		mse::mstd::vector<double> v3(a2, a2+3);
		mse::mstd::vector<double> v4;
#else /*MSVC2010_COMPATIBILE*/
		mse::mstd::vector<double> v2 = { 1.0, 2.0, 3.0 };
		mse::mstd::vector<double> v3 = { 4.0, 5.0, 360 };
		mse::mstd::vector<double> v4;
#endif /*MSVC2010_COMPATIBILE*/
		try {
			v4.insert(v4.begin(), v2.begin(), v3.begin());
		}
		catch (...) {
			std::cerr << "expected exception" << std::endl;
			/* The exception is triggered by a comparision of incompatible "safe" iterators. */
		}
	}

	{
		/* Here we're demonstrating mse::mstd::vector<>'s "java-like" safety due to it's "managed" lifespan.  */

		typedef mse::mstd::vector<int> vint_type;
		mse::mstd::vector<vint_type> vvi;
		{
			vint_type vi;
			vi.push_back(5);
			vvi.push_back(vi);
		}
		auto vi_it = vvi[0].begin();
		vvi.clear();
		/* At this point, the vint_type object is cleared from vvi, but it has not been deallocated/destructed yet because it
		"knows" that there is an iterator, namely vi_it, that is still referencing it. At the moment, std::shared_ptrs are being
		used to achieve this. */
		auto value = (*vi_it); /* So this is actually ok. vi_it still points to a valid item. */
		assert(5 == value);
		vint_type vi2;
		vi_it = vi2.begin();
		/* The vint_type object that vi_it was originally pointing to is now deallocated/destructed, because vi_it no longer
		references it. */
	}

	{
		/* mse::msevector<> is another vector that is highly compatible with std::vector<>. But mse::msevector<> also
		supports a new type of iterator called "ipointer". ipointers make more (intuitive) sense than standard vector
		iterators. Upon insert or delete, ipointers continue to point to the same item, not (necessarily) the same
		position. And they don't become "invalid" upon insert or delete, unless the item they point to is deleted. */

#ifdef MSVC2010_COMPATIBILE
		int a1[4] = { 1, 2, 3, 4 };
		mse::msevector<int> v(a1, a1+4);
#else /*MSVC2010_COMPATIBILE*/
		mse::msevector<int> v = { 1, 2, 3, 4 };
#endif /*MSVC2010_COMPATIBILE*/
		mse::msevector<int>::ipointer ip_vit1(v);
		/*ip_vit1.set_to_beginning();*/ /* This would be redundant as ipointers are set to the beginning at initialization. */
		ip_vit1.advance(2);
		assert(3 == ip_vit1.item());
		auto ip_vit2 = v.ibegin(); /* ibegin() returns an ipointer */
		v.erase(ip_vit2); /* remove the first item */
		assert(3 == ip_vit1.item());
		ip_vit1.set_to_previous();
		assert(2 == ip_vit1.item());
	}

	{
		/* The unsigned types like size_t can cause insidious bugs due to the fact that they can cause signed integers to be
		implicitly converted to unsigned. msetl provides substitutes for size_t and int that change the implicit conversion to
		instead be from unsigned to signed. */

		mse::s_type_test1();

		{
			size_t number_of_security_credits = 0;
			number_of_security_credits += 5;
			int minimum_number_of_security_credits_required_for_access = 7;
			bool access_granted = false;
			if (number_of_security_credits - minimum_number_of_security_credits_required_for_access >= 0) {
				/* You may not even get a compiler warning about the implicit conversion from (signed) int to (unsigned) size_t. */
				access_granted = true; /*oops*/
			}
			else {
				access_granted = false;
				assert(false);
			}
		}

		{
			mse::CSize_t number_of_security_credits; /* initializes to 0 by default */
			number_of_security_credits += 5;
			int minimum_number_of_security_credits_required_for_access = 7;
			bool access_granted = false;
			if (number_of_security_credits - minimum_number_of_security_credits_required_for_access >= 0) {
				access_granted = true;
				assert(false);
			}
			else {
				access_granted = false; /* that's better */
			}
		}

		{
			size_t number_of_security_credits = 0;
			number_of_security_credits += 5;
			mse::CInt minimum_number_of_security_credits_required_for_access = 7;
			mse::CBool access_granted; /* initializes to false by default */
			if (number_of_security_credits - minimum_number_of_security_credits_required_for_access >= 0) {
				access_granted = true;
				assert(false);
			}
			else {
				access_granted = false; /* this works too */
			}
		}

		mse::CSize_t mse_szt1;
		/* size_t szt2 = mse_szt1; */ /* This wouldn't compile. */
#ifdef MSVC2010_COMPATIBILE
		size_t szt1 = mse::as_a_size_t(mse_szt1); /* We exclude automatic conversion from mse::CSize_t to size_t because we
													 consider size_t an intrinsically error prone type. */
#else /*MSVC2010_COMPATIBILE*/
		size_t szt1 = static_cast<size_t>(mse_szt1); /* We exclude automatic conversion from mse::CSize_t to size_t because we
													 consider size_t an intrinsically error prone type. */
#endif /*MSVC2010_COMPATIBILE*/

		try {
			mse::CSize_t mse_szt2;
			mse_szt2 = -3;
		}
		catch (...) {
			std::cerr << "expected exception" << std::endl;
			/* The exception is triggered by an "out of range" assignment to an mse::CSize_t. */
		}

		try {
			mse::CSize_t mse_szt3 = 3;
			mse_szt3 -= 1; /* this is fine */
			mse_szt3 -= 4; /* this is gonna throw an exception */
		}
		catch (...) {
			std::cerr << "expected exception" << std::endl;
			/* The exception is triggered by an attempt to set an mse::CSize_t to an "out of range" value. */
		}
	}

	{
		/* For safety reasons we want to avoid using native pointers. "Managed" pointers like std:shared_ptr are an alternative, but
		sometimes you don't want a pointer that takes ownership (of the object's lifespan). So we provide mse::TRegisteredPointer.
		Because it doesn't take ownership, it can be used with objects allocated on the stack and is compatible with raii
		techniques. Also, in most cases, it can be used as a compatible, direct substitute for native pointers, making it
		straightforward to update legacy code. */

		class A {
		public:
			virtual ~A() {}
			int b = 3;
		};
		class B {
		public:
			static int foo1(A* a_native_ptr) { return a_native_ptr->b; }
			static int foo2(mse::TRegisteredPointer<A> A_registered_ptr) { return A_registered_ptr->b; }
		protected:
			~B() {}
		};

		A* A_native_ptr = nullptr;
		/* mse::TRegisteredPointer<> is basically a "safe" version of the native pointer. */
		mse::TRegisteredPointer<A> A_registered_ptr1;

		{
			A a;
			mse::TRegisteredObj<A> registered_a;
			/* mse::TRegisteredObj<A> is a class that is publicly derived from A, and so should be a compatible substitute for A
			in almost all cases. */

			assert(a.b == registered_a.b);
			A_native_ptr = &a;
			A_registered_ptr1 = &registered_a;
			assert(A_native_ptr->b == A_registered_ptr1->b);

			mse::TRegisteredPointer<A> A_registered_ptr2 = &registered_a;
			A_registered_ptr2 = nullptr;
			try {
				int i = A_registered_ptr2->b; /* this is gonna throw an exception */
			}
			catch (...) {
				std::cerr << "expected exception" << std::endl;
				/* The exception is triggered by an attempt to dereference a null "registered pointer". */
			}

			/* mse::TRegisteredPointers can be coerced into native pointers if you need to interact with legacy code or libraries. */
			B::foo1((A*)A_registered_ptr1);

			if (A_registered_ptr2) {
			}
			else if (A_registered_ptr2 != A_registered_ptr1) {
				A_registered_ptr2 = A_registered_ptr1;
				assert(A_registered_ptr2 == A_registered_ptr1);
			}
		}

		try {
			/* A_registered_ptr1 "knows" that the (registered) object it was pointing to has now been deallocated. */
			int i = A_registered_ptr1->b; /* So this is gonna throw an exception */
		}
		catch (...) {
			std::cerr << "expected exception" << std::endl;
		}

		{
			/* For heap allocations mse::registered_new is kind of analagous to std::make_shared, but again,
			mse::TRegisteredPointers don't take ownership so you are responsible for deallocation. */
			auto A_registered_ptr3 = mse::registered_new<A>();
			assert(3 == A_registered_ptr3->b);
			mse::registered_delete<A>(A_registered_ptr3);
			try {
				/* A_registered_ptr3 "knows" that the (registered) object it was pointing to has now been deallocated. */
				int i = A_registered_ptr3->b; /* So this is gonna throw an exception */
			}
			catch (...) {
				std::cerr << "expected exception" << std::endl;
			}
		}

		{
			/* mse::TRegisteredPointerForLegacy<> behaves very similar to mse::TRegisteredPointer<> but is even more "compatible"
			with native pointers (i.e. less explicit casting is required when interacting with native pointers and native pointer
			interfaces). So if you're updating existing or legacy code to be safer, replacing native pointers with
			mse::TRegisteredPointerForLegacy<> may be more convenient than mse::TRegisteredPointer<>.
			One case where you may need to use mse::TRegisteredPointerForLegacy<> even when not dealing with legacy code is when
			you need a reference to a class before it is fully defined. For example, when you have two classes that mutually
			reference each other. mse::TRegisteredPointer<> does not support this.
			*/

			class C;

			class D {
			public:
				virtual ~D() {}
				mse::TRegisteredPointerForLegacy<C> m_c_ptr;
			};

			class C {
			public:
				mse::TRegisteredPointerForLegacy<D> m_d_ptr;
			};

			mse::TRegisteredObjForLegacy<C> regobjfl_c;
			mse::TRegisteredPointerForLegacy<D> d_ptr = mse::registered_new_for_legacy<D>();

			regobjfl_c.m_d_ptr = d_ptr;
			d_ptr->m_c_ptr = &regobjfl_c;

			mse::registered_delete_for_legacy<D>(d_ptr);
		}

		mse::s_regptr_test1();

		{
			/* Just some simple speed tests. */
			class CE {
			public:
				CE() {}
				CE(int& count_ref) : m_count_ptr(&count_ref) { (*m_count_ptr) += 1; }
				virtual ~CE() { (*m_count_ptr) -= 1; }
				int m_x;
				int *m_count_ptr;
			};
#ifdef _DEBUG
			static const int number_of_loops = 10/*arbitrary*/;
#else /*_DEBUG*/
			static const int number_of_loops = 1000000/*arbitrary*/;
#endif /*_DEBUG*/
			std::cout << "Some simple benchmarks: \n";
			{
				int count = 0;
				auto item_ptr2 = new CE(count);
				delete item_ptr2; item_ptr2 = nullptr;
				auto t1 = std::chrono::high_resolution_clock::now();
				for (int i = 0; i < number_of_loops; i += 1) {
					auto item_ptr = new CE(count);
					item_ptr2 = item_ptr;
					delete item_ptr;
					item_ptr = nullptr;
				}

				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "native pointer: " << time_span.count() << " seconds.";
				if (0 != count) {
					std::cout << " destructions pending: " << count << "."; /* Using the count variable for (potential) output should prevent the optimizer from discarding it. */
				}
				std::cout << std::endl;
			}
			{
				int count = 0;
				auto item_ptr2 = mse::registered_new<CE>(count);
				mse::registered_delete<CE>(item_ptr2);
				auto t1 = std::chrono::high_resolution_clock::now();
				for (int i = 0; i < number_of_loops; i += 1) {
					auto item_ptr = mse::registered_new<CE>(count);
					item_ptr2 = item_ptr;
					mse::registered_delete<CE>(item_ptr);
				}

				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "mse::TRegisteredPointer: " << time_span.count() << " seconds.";
				if (0 != count) {
					std::cout << " destructions pending: " << count << "."; /* Using the count variable for (potential) output should prevent the optimizer from discarding it. */
				}
				std::cout << std::endl;
			}
			{
				int count = 0;
				auto item_ptr2 = mse::registered_new_for_legacy<CE>(count);
				mse::registered_delete_for_legacy<CE>(item_ptr2);
				auto t1 = std::chrono::high_resolution_clock::now();
				for (int i = 0; i < number_of_loops; i += 1) {
					auto item_ptr = mse::registered_new_for_legacy<CE>(count);
					item_ptr2 = item_ptr;
					mse::registered_delete_for_legacy<CE>(item_ptr);
				}

				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "mse::TRegisteredPointerForLegacy: " << time_span.count() << " seconds.";
				if (0 != count) {
					std::cout << " destructions pending: " << count << "."; /* Using the count variable for (potential) output should prevent the optimizer from discarding it. */
				}
				std::cout << std::endl;
			}
			{
				int count = 0;
				auto item_ptr2 = std::make_shared<CE>(count);
				auto t1 = std::chrono::high_resolution_clock::now();
				for (int i = 0; i < number_of_loops; i += 1) {
					auto item_ptr = std::make_shared<CE>(count);
					item_ptr2 = item_ptr;
					item_ptr = nullptr;
				}
				item_ptr2 = nullptr;

				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "std::shared_ptr: " << time_span.count() << " seconds.";
				if (0 != count) {
					std::cout << " destructions pending: " << count << "."; /* Using the count variable for (potential) output should prevent the optimizer from discarding it. */
				}
				std::cout << std::endl;
			}
			{
				int count = 0;
				auto item_ptr2 = &(mse::TRegisteredObj<CE>(count));
				auto t1 = std::chrono::high_resolution_clock::now();
				{
					for (int i = 0; i < number_of_loops; i += 1) {
						mse::TRegisteredObj<CE> object(count);
						auto item_ptr = &object;
						item_ptr2 = item_ptr;
					}
				}

				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "mse::TRegisteredPointer targeting the stack: " << time_span.count() << " seconds.";
				if (0 != count) {
					std::cout << " destructions pending: " << count << "."; /* Using the count variable for (potential) output should prevent the optimizer from discarding it. */
				}
				std::cout << std::endl;
			}

			std::cout << std::endl;
			static const int number_of_loops2 = (10/*arbitrary*/)*number_of_loops;
			{
				class CF {
				public:
					CF(int a = 0) : m_a(a) {}
					CF* m_next_item_ptr;
					int m_a = 3;
				};
				CF item1(1);
				CF item2(2);
				CF item3(3);
				item1.m_next_item_ptr = &item2;
				item2.m_next_item_ptr = &item3;
				item3.m_next_item_ptr = &item1;
				auto t1 = std::chrono::high_resolution_clock::now();
				CF* cf_ptr = item1.m_next_item_ptr;
				for (int i = 0; i < number_of_loops2; i += 1) {
					cf_ptr = cf_ptr->m_next_item_ptr;
				}
				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "native pointer dereferencing: " << time_span.count() << " seconds.";
				if (3 == cf_ptr->m_a) {
					std::cout << " "; /* Using cf_ptr->m_a for (potential) output should prevent the optimizer from discarding too much. */
				}
				std::cout << std::endl;
			}
			{
				class CF {
				public:
					CF(int a = 0) : m_a(a) {}
					mse::TRegisteredPointerForLegacy<CF> m_next_item_ptr;
					int m_a = 3;
				};
				mse::TRegisteredObjForLegacy<CF> item1(1);
				mse::TRegisteredObjForLegacy<CF> item2(2);
				mse::TRegisteredObjForLegacy<CF> item3(3);
				item1.m_next_item_ptr = &item2;
				item2.m_next_item_ptr = &item3;
				item3.m_next_item_ptr = &item1;
				auto t1 = std::chrono::high_resolution_clock::now();
				mse::TRegisteredPointerForLegacy<CF>* rpfl_ptr = std::addressof(item1.m_next_item_ptr);
				for (int i = 0; i < number_of_loops2; i += 1) {
					rpfl_ptr = std::addressof((*rpfl_ptr)->m_next_item_ptr);
				}
				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "mse::TRegisteredObjForLegacy (checked) dereferencing: " << time_span.count() << " seconds.";
				if (3 == (*rpfl_ptr)->m_a) {
					std::cout << " "; /* Using rpfl_ref->m_a for (potential) output should prevent the optimizer from discarding too much. */
				}
				std::cout << std::endl;
			}
			{
				class CF {
				public:
					CF(int a = 0) : m_a(a) {}
					mse::TRegisteredPointerForLegacy<CF> m_next_item_ptr;
					int m_a = 3;
				};
				mse::TRegisteredObjForLegacy<CF> item1(1);
				mse::TRegisteredObjForLegacy<CF> item2(2);
				mse::TRegisteredObjForLegacy<CF> item3(3);
				item1.m_next_item_ptr = &item2;
				item2.m_next_item_ptr = &item3;
				item3.m_next_item_ptr = &item1;
				auto t1 = std::chrono::high_resolution_clock::now();
				CF* cf_ptr = (item1.m_next_item_ptr).raw_pointer();
				for (int i = 0; i < number_of_loops2; i += 1) {
					cf_ptr = (cf_ptr->m_next_item_ptr).raw_pointer();
				}
				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "mse::TRegisteredObjForLegacy unchecked dereferencing: " << time_span.count() << " seconds.";
				if (3 == cf_ptr->m_a) {
					std::cout << " "; /* Using rpfl_ref->m_a for (potential) output should prevent the optimizer from discarding too much. */
				}
				std::cout << std::endl;
			}
			{
				class CF {
				public:
					CF(int a = 0) : m_a(a) {}
					std::weak_ptr<CF> m_next_item_ptr;
					int m_a = 3;
				};
				auto item1_ptr = std::make_shared<CF>(1);
				auto item2_ptr = std::make_shared<CF>(2);
				auto item3_ptr = std::make_shared<CF>(3);
				item1_ptr->m_next_item_ptr = item2_ptr;
				item2_ptr->m_next_item_ptr = item3_ptr;
				item3_ptr->m_next_item_ptr = item1_ptr;
				auto t1 = std::chrono::high_resolution_clock::now();
				std::weak_ptr<CF>* wp_ptr = &(item1_ptr->m_next_item_ptr);
				for (int i = 0; i < number_of_loops2; i += 1) {
					wp_ptr = &((*wp_ptr).lock()->m_next_item_ptr);
				}
				auto t2 = std::chrono::high_resolution_clock::now();
				auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
				std::cout << "std::weak_ptr dereferencing: " << time_span.count() << " seconds.";
				if (3 == (*wp_ptr).lock()->m_a) {
					std::cout << " "; /* Using wp_ref.lock()->m_a for (potential) output should prevent the optimizer from discarding too much. */
				}
				std::cout << std::endl;
			}
		}
	}

	{
		/* Stl provides a copyable, assignable wrapper for C++ references called std::reference_wrapper. std::reference_wrapper,
		like native C++ references, are not completely safe in the sense that the object they refer to can be deallocated while
		a reference to it is still available. So we provide mse::TRegisteredRefWrapper, a safe implementation of
		std::reference_wrapper that "knows" when the object being referenced has been deallocated and will throw an exception
		on any attempt to access the object after it has been destroyed. */
		{
			/* This example originally comes from http://en.cppreference.com/w/cpp/utility/functional/reference_wrapper. */
			std::list<mse::TRegisteredObj<mse::CInt>> l(10);
			std::iota(l.begin(), l.end(), -4);

			std::vector<mse::TRegisteredRefWrapper<mse::CInt>> v(l.begin(), l.end());
			// can't use shuffle on a list (requires random access), but can use it on a vector
			std::shuffle(v.begin(), v.end(), std::mt19937{ std::random_device{}() });

			std::cout << '\n';
			std::cout << "TRegisteredRefWrapper test output: \n";
			std::cout << "Contents of the list: ";
			for (auto n : l) std::cout << n << ' '; std::cout << '\n';

			std::cout << "Contents of the list, as seen through a shuffled vector: ";
			for (auto i : v) {
				mse::CInt i2 = i;
				std::cout << i2 << ' ';
			}
			std::cout << '\n';

			std::cout << "Doubling the values in the initial list...\n";
			for (auto& i : l) {
				i *= 2;
			}

			std::cout << "Contents of the list, as seen through a shuffled vector: ";
			for (auto i : v) {
				mse::CInt i2 = i;
				std::cout << i2 << ' ';
			}
			std::cout << '\n';
			std::cout << '\n';
		}
	}

	return 0;
}
