#ifndef BITCOIN_WALLET_COINCONTROL_H
#define BITCOIN_WALLET_COINCONTROL_H

/** Coin Control Features. */
class CCoinControl
{
public:
    CTxDestination destChange;
    bool fAllowWatchOnly;

    CCoinControl()
    {
        SetNull();
    }

    void SetNull()
    {
        destChange = CNoDestination();
        fAllowWatchOnly = false;
        setSelected.clear();
    }
    
    bool HasSelected() const
    {
        return (setSelected.size() > 0);
    }
    
    bool IsSelected(const uint256& hash, unsigned int n) const
    {
        COutPoint outpt(hash, n);
        return (setSelected.count(outpt) > 0);
    }
    
    void Select(const COutPoint& output)
    {
        setSelected.insert(output);
    }
    
    void UnSelect(const COutPoint& output)
    {
        setSelected.erase(output);
    }
    
    void UnSelectAll()
    {
        setSelected.clear();
    }

    void ListSelected(std::vector<COutPoint>& vOutpoints)
    {
        vOutpoints.assign(setSelected.begin(), setSelected.end());
    }
        
private:
    std::set<COutPoint> setSelected;

};

#endif // BITCOIN_WALLET_COINCONTROL_H
