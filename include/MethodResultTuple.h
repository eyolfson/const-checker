#ifndef CLANG_IMMUTABILITY_CHECK_METHOD_RESULT_TUPLE_H
#define CLANG_IMMUTABILITY_CHECK_METHOD_RESULT_TUPLE_H

#include <clang/AST/DeclCXX.h>

namespace clang {
namespace immutability {

enum class MutateResult: uint32_t {
  NO_MUTATION = 1,
  MAYBE_MUTATION = 2,
};

enum class ReturnResult: uint32_t {
  NOOP = 1,
  FIELD_TRANSITIVE = 2,
  FIELD_NON_TRANSITIVE = 3,
  OTHER = 4,
};

class MethodResultTuple {
public:
  MutateResult mutateResult;
  ReturnResult returnResult;

  MethodResultTuple() : mutateResult(MutateResult::MAYBE_MUTATION),
			returnResult(ReturnResult::OTHER) {}
  MethodResultTuple(MutateResult m, ReturnResult r)
  : mutateResult(m), returnResult(r) {}
};

}
}

#endif
