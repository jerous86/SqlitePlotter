#pragma once

#include <QList>

template<class FROM, class TO, class FUN>
QList<TO> map(const QList<FROM> &xs, FUN fun) {
    QList<TO> ret;
    for(auto &x:xs) { ret<<fun(x); }
    return ret;
}
template<class T>
T sum(const QList<T> &xs) {
    T ret=T(0);
    for(auto &x:xs) { ret+=x; }
    return ret;
}

template<class T=float>
struct WeightedMean {
    unsigned n=0;
    T mean=T(0);
    T variance=T(0);
    T min=T(0);
    T max=T(0);
    T sum=T(0);
    T weight_sum=0;

    void collect(const T x, const T weight) {
        n++;
        const T old_wsum=weight_sum;
        weight_sum+=weight;

        const T delta=(x)-(mean);
        const T incr=(weight/weight_sum) * delta; // (93)
        mean = mean + T(incr); // (93)
        variance = T(
            (old_wsum>0 ? (weight_sum-weight)/old_wsum * (variance) : 0) +
            (weight * /*old delta*/delta * /*new delta*/((x)-(mean)))
            ); // (113)
        sum+=x;

        if (n==1) { min=max=x; } else { min=std::min(x,min); max=std::max(x,max); }
    }
};
