#include "AlgorithmRegistrar.h"

AlgorithmRegistrar AlgorithmRegistrar::singleton_;

AlgorithmRegistrar& AlgorithmRegistrar::get() {
    return singleton_;
}
