#ifndef COSTFUNCTIONCACHE_H
#define COSTFUNCTIONCACHE_H

#include "coinamount.h"

#include <QString>
#include <QCache>

class CostFunctionCache
{
public:
    explicit CostFunctionCache();

    Coin getY( const Coin &profile_u, const Coin &reserve, const Coin &x );
    const Coin &getTicksize() const { return m_ticksize; }
    const Coin &getMaxX() const { return m_max_x; }

private:
    Coin m_ticksize, m_max_x;
    QHash<QString,Coin> m_cache;
};

#endif // COSTFUNCTIONCACHE_H
