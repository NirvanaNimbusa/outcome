#include "../../../include/boost/outcome.hpp"
#include <iostream>

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

//! [decl_f_g]
auto f() -> int;  // may throw
auto g() noexcept -> outcome::result<int>;
//! [decl_f_g]

auto f() -> int
{
  throw std::runtime_error{"failure"};
}

auto g() noexcept -> outcome::result<int>
{
  return 1;
}

namespace old
{
    auto h() noexcept -> outcome::outcome<int>;
}

//! [def_h]
auto old::h() noexcept -> outcome::outcome<int>
{
  BOOST_OUTCOME_TRY(i, g());               // #1
    
  try {
    return i + f();
  }
  catch (...) {
    return std::current_exception(); // #2
  }
}
//! [def_h]

//! [def_z]
auto z() -> int // may throw
{
  if (outcome::outcome<int> o = old::h())
    return o.value();
  else if (o.has_exception())
    std::rethrow_exception(o.exception());
  else
    throw std::system_error{o.error()};
}
//! [def_z]

int main()
try {
  return z();
}
catch(std::runtime_error const& e) {
  std::cerr << e.what() << std::endl;
  return 1;
}