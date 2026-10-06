#ifndef MATCHING_STRATEGY_H
#define MATCHING_STRATEGY_H

#include "student.h"

class MatchingStrategy {
public:
    virtual ~MatchingStrategy() {}

    virtual double calculateSimilarity(
        const Student& student1,
        const Student& student2
    ) const = 0;
};

#endif
