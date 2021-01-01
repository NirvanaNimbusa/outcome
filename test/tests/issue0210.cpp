/* Unit testing for outcomes
(C) 2013-2021 Niall Douglas <http://www.nedproductions.biz/> (1 commit)


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <boost/outcome/outcome.hpp>
#include <boost/outcome/try.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>

namespace issues210
{
  namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

  struct error
  {
    boost::system::error_code code;
  };

  // say that my custom error code type is error code compatible
  inline boost::system::error_code make_error_code(error error) noexcept { return error.code; }

  template <typename T> using custom_result = outcome::basic_result<T, error, outcome::policy::default_policy<T, error, void>>;

  // source of custom result type with error code compatible error type
  inline custom_result<int> funcA(int x) { return x; }

  // Is the custom result type explicitly constructible to an ordinary result type?
  inline outcome::result<int> funcB(int x) { return outcome::result<int>(funcA(x)); }

  // Does the custom result type TRY-convert to an ordinary result type?
  inline outcome::result<int> func1(int x)
  {
    BOOST_OUTCOME_TRY(auto &&y, funcA(x));
    return funcB(y);
  }

  // Is the custom result type explicitly constructible to an ordinary outcome type?
  inline outcome::outcome<int> funcC(int x) { return outcome::outcome<int>(funcA(x)); }

  // Does the custom result type TRY-convert to an ordinary outcome type?
  inline outcome::outcome<int> func2(int x)
  {
    BOOST_OUTCOME_TRY(auto &&y, funcA(x));
    return funcC(y);
  }
}  // namespace issues210


BOOST_OUTCOME_AUTO_TEST_CASE(issues_0210_test, "result<int, E> with error code compatible custom E does not TRY into a result<int, boost::system::error_code> function")
{
  BOOST_CHECK(issues210::func1(5).value() == 5);
  BOOST_CHECK(issues210::func2(5).value() == 5);
}
