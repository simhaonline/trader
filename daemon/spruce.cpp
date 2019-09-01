#include "spruce.h"
#include "coinamount.h"
#include "global.h"


Spruce::Spruce()
{
    /// user settings
    m_hedge_target = "0.95"; // keep our market valuations at most 1-x% apart
    m_order_greed = "0.99"; // keep our spread at least 1-x% apart

    m_long_max = "0.3000000"; // max long total
    m_short_max = "-0.50000000"; // max short total
    m_market_max = "0.20000000";
    m_order_size = "0.00500000";
    m_order_nice = "2";
    m_trailing_price_limit = "0.96";

    /// per-exchange constants
    m_order_size_min = "0.00070000"; // TODO: scale this minimum to each exchange

    /// internal
    m_log_map_end = Coin( CoinAmount::COIN * 100 );
    m_leverage = CoinAmount::COIN;

    /// cost function image accuracy
    /// 0.001 = good accuracy, 13MB cache
    /// 0.0001 = great accuracy, 130MB cache
    m_tick_size = "0.0001";

    /// change profile u of cost function
    m_profile_u = "10";
}

Spruce::~Spruce()
{
    while ( nodes_start.size() > 0 )
        delete nodes_start.takeFirst();

    clearLiveNodes();
}

void Spruce::mapCostFunctionImage()
{
    m_cost_function_image.clear();

    kDebug() << "[Spruce] generating cost function image...";
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    // for 0 to m_log_map_end, subtract (1-y)/u from y for every u% increase
    // y += ( 1 - y ) * profile_u;
    Coin y;
    const Coin profile = m_tick_size * 10 / m_profile_u;
    for ( Coin x; x <= m_log_map_end; x += m_tick_size /*granularity to find y*/ )
    {
        if ( !x.isZero() ) // don't skip zero, just set zero to zero
            y += ( CoinAmount::COIN - y ) * profile;

        m_cost_function_image.insert( x, y );
    }

    kDebug() << "[Spruce] done generating cost function image with" << m_cost_function_image.size() <<
                "points. took" << QDateTime::currentMSecsSinceEpoch() - t0 << "ms.";
}

void Spruce::setCurrencyWeight( QString currency, Coin weight )
{
    // clear by coin
    for ( QMultiMap<Coin,QString>::const_iterator i = currency_weight_by_coin.begin(); i != currency_weight_by_coin.end(); i++ )
    {
        if ( i.value() == currency )
        {
            currency_weight_by_coin.remove( i.key(), i.value() );
            break;
        }
    }

    currency_weight[ currency ] = weight;
    currency_weight_by_coin.insert( weight, currency );
}

Coin Spruce::getMarketWeight( QString market ) const
{
    for ( QList<QString>::const_iterator i = getCurrencies().begin(); i != getCurrencies().end(); i++ )
    {
        const QString &currency = *i;
        const QString market_recreated = getBaseCurrency() + "-" + currency;

        if ( market == market_recreated )
            return currency_weight.value( currency );
    }

    return Coin();
}

void Spruce::addStartNode( QString _currency, QString _quantity, QString _price )
{
    Node *n = new Node();
    n->currency = _currency;
    n->quantity = _quantity;
    n->price = _price;
    n->recalculateAmountByQuantity();

    original_quantity[ _currency ] = _quantity;

    nodes_start += n;
}

void Spruce::addLiveNode( QString _currency, QString _price )
{
    Node *n = new Node();
    n->currency = _currency;
    n->price = _price;

    nodes_now += n;
}

void Spruce::clearLiveNodes()
{
    while ( nodes_now.size() > 0 )
        delete nodes_now.takeFirst();
}

void Spruce::calculateAmountToShortLong()
{
    normalizeEquity();
    equalizeDates();

    // record amount to shortlong in a map and get total
    m_amount_to_shortlong_map.clear();
    m_amount_to_shortlong_total = Coin();

    QList<QString> markets = getMarkets();
    for ( QList<QString>::const_iterator i = markets.begin(); i != markets.end(); i++ )
    {
        const QString &market = *i;
        const Coin &shortlong_market = getAmountToShortLongNow( market );

        m_amount_to_shortlong_map[ market ] = shortlong_market;
        m_amount_to_shortlong_total += shortlong_market;
    }
}

Coin Spruce::getAmountToShortLongNow( QString market )
{
    if ( !amount_to_shortlong.contains( market ) )
        return Coin();

    Coin ret = -amount_to_shortlong.value( market ) + shortlonged_total.value( market );

    return ret;
}

void Spruce::addToShortLonged( QString market, Coin amount )
{
    shortlonged_total[ market ] += amount;
}

QList<QString> Spruce::getCurrencies() const
{
    return original_quantity.keys();
}

QList<QString> Spruce::getMarkets() const
{
    // TODO: adapt this to each exchange
    QList<QString> ret;
    const QList<QString> &keys = original_quantity.keys();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
        ret += base_currency + "-" + *i;

    return ret;
}

QString Spruce::getSaveState()
{
    QString ret;

    // save base
    ret += QString( "setsprucebasecurrency %1\n" ).arg( base_currency );

    // save log factor
    ret += QString( "setspruceleverage %1\n" ).arg( m_leverage );

    // save profile u
    ret += QString( "setspruceprofile %1\n" ).arg( m_profile_u );

    // save hedge target
    ret += QString( "setsprucehedgetarget %1\n" ).arg( m_hedge_target );

    // save order greed
    ret += QString( "setspruceordergreed %1\n" ).arg( m_order_greed );

    // save long max
    ret += QString( "setsprucelongmax %1\n" ).arg( m_long_max );

    // save short max
    ret += QString( "setspruceshortmax %1\n" ).arg( m_short_max );

    // save market max
    ret += QString( "setsprucemarketmax %1\n" ).arg( m_market_max );

    // save order size
    ret += QString( "setspruceordersize %1\n" ).arg( m_order_size );

    // save order size
    ret += QString( "setspruceordernice %1\n" ).arg( m_order_nice );

    // save order trailing limit
    ret += QString( "setspruceordertrail %1\n" ).arg( m_trailing_price_limit );

    // save market weights
    for ( QMap<QString,Coin>::const_iterator i = currency_weight.begin(); i != currency_weight.end(); i++ )
    {
        ret += QString( "setspruceweight %1 %2\n" )
                .arg( i.key() )
                .arg( i.value() );
    }

    // save start nodes
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;

        ret += QString( "setsprucestartnode %1 %2 %3\n" )
                .arg( n->currency )
                .arg( original_quantity.value( n->currency ) )
                .arg( n->price );
    }

    // save shortlonged_total
    for ( QMap<QString,Coin>::const_iterator i = shortlonged_total.begin(); i != shortlonged_total.end(); i++ )
    {
        ret += QString( "setspruceshortlongtotal %1 %2\n" )
                .arg( i.key() )
                .arg( i.value() );
    }

    return ret;
}

void Spruce::setProfileU( Coin u )
{
    m_profile_u = u;
    m_cost_function_image.clear(); // clear image
}

Coin Spruce::getEquityNow( QString currency )
{
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;

        if ( n->currency == currency )
            return n->quantity * n->price;
    }

    return Coin();
}

Coin Spruce::getLastCoeffForMarket( const QString &market ) const
{
    int idx = market.indexOf( QChar('-') );
    if ( idx < 0 )
        return Coin();

    // TODO: fix this for more exchanges
    QString currency = market.mid( idx +1, market.size() - idx );

    if ( !m_last_coeffs.contains( currency ) )
        qDebug() << "local warning: can't find coeff for currency" << currency;

    return m_last_coeffs.value( currency );
}

void Spruce::equalizeDates()
{
    // if the function image map is empty, initialize it
    if ( m_cost_function_image.isEmpty() )
        mapCostFunctionImage();

    // if it's still empty, complain
    if ( m_cost_function_image.isEmpty()  )
    {
        kDebug() << "local error: couldn't map function image";
        return;
    }

    /// psuedocode
    //
    // get initial coeffs
    // find hi/lo
    // while hi.ratio(0.99) > lo
    //     shortlongs[ highest coeff market ] -= 100k sat
    //     shortlongs[ lowest coeff market ] += 100k sat
    //     get new coeff, set new hi/lo
    ///

    // ensure dates exist
    if ( nodes_start.size() != nodes_now.size() )
    {
        qDebug() << "error: couldn't find one of the dates";
        return;
    }

    // track shorts/longs
    QMap<QString,Coin> shortlongs;

    // find hi/lo coeffs
    m_start_coeffs = m_relative_coeffs = getRelativeCoeffs();

    // avoid infinite loop
    if ( m_hedge_target > Coin( "0.997" ) )
        m_hedge_target = "0.997";

    const Coin min_adjustment = CoinAmount::SATOSHI * 50000;
    const Coin hi_equity = getEquityNow( m_relative_coeffs.hi_currency );
    const Coin ticksize = std::max( min_adjustment, hi_equity / 10000 );

    // if we don't have enough to make the adjustment, abort
    if ( hi_equity < min_adjustment )
    {
        kDebug() << "local warning: not enough equity to equalizeDates" << hi_equity;
        return;
    }

//    kDebug() << "hi_coeff:" << m_relative_coeffs.hi_coeff << m_relative_coeffs.hi_currency
//             << "lo_coeff:" << m_relative_coeffs.lo_coeff << m_relative_coeffs.lo_currency;
//    kDebug() << "ticksize" << ticksize;
//    kDebug() << "hi_equity" << hi_equity;

    qint64 i = 0;
    while ( m_relative_coeffs.hi_coeff * m_hedge_target > m_relative_coeffs.lo_coeff )
    {
        if ( i++ == 10001 ) // safety break
            break;

        // find highest/lowest coeff market
        for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
        {
            Node *n = *i;

            if ( n->currency == m_relative_coeffs.hi_currency &&
                 n->amount > ticksize ) // check if we have enough to short
            {
                shortlongs[ n->currency ] -= ticksize * m_leverage;
                n->amount -= ticksize;
            }
            else if ( n->currency == m_relative_coeffs.lo_currency )
            {
                shortlongs[ n->currency ] += ticksize * m_leverage;
                n->amount += ticksize;
            }
            else
            {
                continue;
            }

            n->recalculateQuantityByPrice();
        }

        m_relative_coeffs = getRelativeCoeffs();

//        kDebug() << "hi_coeff" << m_relative_coeffs.hi_coeff
//                 << "lo_coeff" << m_relative_coeffs.lo_coeff;
    }

    // flip values, because we want shorts as positive and longs as negative
    for ( QMap<QString,Coin>::const_iterator i = shortlongs.begin(); i != shortlongs.end(); i++ )
    {
        QString market = i.key();

        // TODO: adapt this to each exchange
        market.prepend( base_currency + "-" );

        amount_to_shortlong[ market ] = i.value();
    }
}

void Spruce::normalizeEquity()
{
    if ( nodes_start.size() != nodes_now.size() )
    {
        qDebug() << "local error: spruce: start node count not equal date1 node count";
        return;
    }

    Coin total, original_total, total_scaled;

    // step 1: calculate total equity
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        total += n->quantity * n->price;
    }
    original_total = total;

    // step 2: calculate mean equity if we were to weight each market the same
    QMap<QString,Coin> mean_equity_for_market;

    Coin mean_equity = total / nodes_start.size();
    mean_equity.truncateByTicksize( "0.00000001" ); // toss subsatoshi digits
    //qDebug() << "starting equity:" << total << "mean:" << mean_equity;

    // step 3: calculate weighted equity from lowest to highest weight (multimap is sorted by weight)
    //         for each market and recalculate mean/total equity
    int ct = nodes_start.size();
    for ( QMultiMap<Coin,QString>::const_iterator i = currency_weight_by_coin.begin(); i != currency_weight_by_coin.end(); i++ )
    {
        const QString &currency = i.value();
        const Coin &weight = i.key();
        const Coin equity_to_use = mean_equity * weight;

        mean_equity_for_market.insert( currency, equity_to_use );

        //qDebug() << "equity scaled for" << currency << equity_to_use;
        total_scaled += equity_to_use; // record equity to ensure total_scaled == original total

        // if there isn't a last item, exit ehre
        if ( --ct == 0 ) break;

        // do some things to help next iteration, recalculate mean equity based on amount used
        total -= equity_to_use;
        mean_equity = total / ct;
    }
    //assert( total_scaled == original_total );

    if ( total_scaled != original_total )
    {
        qDebug() << "local error: spruce: total_scaled != original total (check number of spruce markets)";
        return;
    }

//    qDebug() << "equity used:" << total_scaled;
//    qDebug() << "equity available:" << original_total;

    // step 4: apply mean equity for each market
    QMap<QString,Coin> start_quantities; // cache date1 quantity to store in date2

    // calculate new equity for all dates: e = mean / price
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        n->amount = mean_equity_for_market.value( n->currency, CoinAmount::COIN );
        n->recalculateQuantityByPrice();
        start_quantities.insert( n->currency, n->quantity );
        //qDebug() << n->currency << "quantity is now" << n->quantity;
    }

    // step 5: put the mean adjusted date1 quantites into date2. after this step, we can figure out the new "normalized" valuations
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;
        n->quantity = start_quantities.value( n->currency );
        n->recalculateAmountByQuantity();
        //qDebug() << n->currency << "quantity is now" << n->quantity;
    }
}

QMap<QString, Coin> Spruce::getMarketCoeffs()
{
    QMap<QString/*currency*/,Coin> start_scores, relative_coeff;

    // calculate start scores
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        start_scores.insert( n->currency,  n->quantity * n->price );
    }

    // calculate new score based on starting score using a loss function
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;
        const Coin score = n->quantity * n->price;
        const Coin &start_score = start_scores.value( n->currency );
        Coin &new_coeff = relative_coeff[ n->currency ];

        bool is_negative = score < start_score;

        Coin transformed_score = is_negative ? start_score / score
                                             : score / start_score;

        transformed_score.truncateByTicksize( m_tick_size );
        transformed_score -= CoinAmount::COIN;

        // clamp score above maximum
        if ( transformed_score >= m_log_map_end )
            transformed_score = m_log_map_end;

        new_coeff = is_negative ? -m_cost_function_image.value( transformed_score )
                                :  m_cost_function_image.value( transformed_score );
    }

    return relative_coeff;
}

RelativeCoeffs Spruce::getRelativeCoeffs()
{
    // get coeffs for time distances of balances
    m_last_coeffs = getMarketCoeffs();

    // find the highest and lowest coefficents
    RelativeCoeffs ret;
    for ( QMap<QString,Coin>::const_iterator i = m_last_coeffs.begin(); i != m_last_coeffs.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &coeff = i.value();

        if ( coeff > ret.hi_coeff )
        {
            ret.hi_coeff  = coeff;
            ret.hi_currency = currency;
        }

        if ( coeff < ret.lo_coeff )
        {
            ret.lo_coeff  = coeff;
            ret.lo_currency = currency;
        }
    }

    return ret;
}
