#pragma once



#ifndef __XSTD_RESULT_H_GUARD
#define __XSTD_RESULT_H_GUARD



namespace xstd
{

	template <typename ValueType>
	struct result
	{
		bool _Success = false;
		ValueType _Value;

		explicit operator bool() const noexcept { return _Success; }

		T& value() {
			assert(_Success && "result::value — accessed value on failed result");
			return _Value;
		}

		const T& value() const {
			assert(_Success && "...");
			return _Value;
		}

		static result Success(T value) { return result{ true, std::move(value) }; }
		static result Fail() { return result{ false, T{} }; }
	};

}

#endif
