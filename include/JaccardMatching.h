#ifndef JACCARD_MATCHING_H
#define JACCARD_MATCHING_H
#include "MatchingStrategy.h"
class JaccardMatching : public MatchingStrategy {
public:
    double calculateSimilarity(
        const Student& student1,
        const Student& student2
    ) const override;
};
#endif
