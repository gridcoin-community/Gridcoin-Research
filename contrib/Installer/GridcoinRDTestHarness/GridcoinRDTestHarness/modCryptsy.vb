

Public Class Cryptsy

    Private _APIKey As String
    Private _Secret As String


    Public Function createTimeStamp(datestr As String, format As String)
        Return String.Format(format, datestr)
    End Function

    Public Sub New(APIKey As String, Secret As String)

        _APIKey = APIKey
        _Secret = Secret

    End Sub

    Public Function post_process()
        '//# Add timestamps if there isnt one but is a datetime
    End Function
    Public Function UrlOpen(sURL As String) As String

    End Function
    Public Function JsonElement(data As String, element As String) As String

    End Function
    Public Function api_query(method As String, req As String) As String

        Dim result As String

        If (method = "marketdata" Or method = "orderdata" Or method = "marketdatav2") Then
            result = UrlOpen("http://pubapi.cryptsy.com/api.php?method=" + method)

            'Return json.loads(ret.read())
            Return result

        ElseIf (method = "singlemarketdata" Or method = "singleorderdata") Then
            result = UrlOpen("http://pubapi.cryptsy.com/api.php?method=" + method + "&marketid=" + Trim(JsonElement(req, "marketid")))

            Return result
        Else

            req = Replace(req, "{method}", method)
            req = Replace(req, "{nonce}", Now.Millisecond)


            'UpdateJson(req, "method", method)
            'UpdateJson(req, "nonce", Now.Millisecond)
            Dim post_data As String

            post_data = System.Net.WebUtility.HtmlEncode(req)

            Dim sign As String
            sign = HMACSHA512Hasher.HexHash(post_data, _APIKey)

            Dim sURL As String = "https://www.cryptsy.com/api"
            Dim responsebody As String = ""
            Using client As New Net.WebClient
                Dim reqparm As New Specialized.NameValueCollection
                reqparm.Add("method", "getmarketdata")
                client.Headers.Add("Sign", sign)
                client.Headers.Add("Key", _APIKey)
                Dim responsebytes = client.UploadValues(sURL, "POST", reqparm)
                responsebody = (New System.Text.UTF8Encoding).GetString(responsebytes)
            End Using

            'result = UrlOpen("https://www.cryptsy.com/api", post_data, headers)

            'jsonRet = json.loads(ret.read())

            Return responsebody


        End If

    End Function

    Public Function getMarketData()
        Return api_query("marketdata", "")
    End Function


    Public Function getMarketDataV2()
        Return api_query("marketdatav2", "")
    End Function


    Public Function getSingleMarketData(marketid)
        Return api_query("singlemarketdata", "{'marketid': marketid}")
    End Function

    Public Function getOrderbookData(marketid)
        If (marketid = "") Then

            Return api_query("orderdata", "")
        End If

        Return api_query("singleorderdata", "{'marketid': marketid}")


    End Function


        '    # Outputs: 
        '    # balances_available  Array of currencies and the balances availalbe for each
        '   # balances_hold   Array of currencies and the amounts currently on hold for open orders
        '   # servertimestamp Current server timestamp
        '   # servertimezone  Current timezone for the server
        '   # serverdatetime  Current date/time on the server
        '  # openordercount  Count of open orders on your account

    Public Function getInfo(self)
        Return self.api_query("getinfo", "")

    End Function


    '# Outputs: Array of Active Markets 
    '# marketid    Integer value representing a market
    '# label   Name for this market, for example: AMC/BTC
    '# primary_currency_code   Primary currency code, for example: AMC
    '# primary_currency_name   Primary currency name, for example: AmericanCoin
    '# secondary_currency_code Secondary currency code, for example: BTC
    '# secondary_currency_name Secondary currency name, for example: BitCoin
    '# current_volume  24 hour trading volume in this market
    '# last_trade  Last trade price for this market
    '# high_trade  24 hour highest trade price in this market
    '# low_trade   24 hour lowest trade price in this market

    Public Function getMarkets()
        Return api_query("getmarkets", "")
    End Function




    '# Outputs: Array of Deposits and Withdrawals on your account 
    '# currency    Name of currency account
    '# timestamp   The timestamp the activity posted
    '# datetime    The datetime the activity posted
    '# timezone    Server timezone
    '# type    Type of activity. (Deposit / Withdrawal)
    '# address Address to which the deposit posted or Withdrawal was sent
    '# amount  Amount of transaction
    Public Function myTransactions()
        Return api_query("mytransactions", "")

    End Function
        



    '    # Inputs:
    '    # marketid    Market ID for which you are querying
    '    ##
    '    # Outputs: Array of last 1000 Trades for this Market, in Date Decending Order 
    '    # datetime    Server datetime trade occurred
    '    # tradeprice  The price the trade occurred at
    '    # quantity    Quantity traded
    '    # total   Total value of trade (tradeprice * quantity)
    Public Function marketTrades(marketid)
        Return api_query("markettrades", "{'marketid': marketid}")

    End Function
        



    '    # Inputs:
    '    # marketid    Market ID for which you are querying
    '    ##
    '    # Outputs: 2 Arrays. First array is sellorders listing current open sell orders ordered price ascending. Second array is buyorders listing current open buy orders ordered price descending. 
    '    # sellprice   If a sell order, price which order is selling at
    '    # buyprice    If a buy order, price the order is buying at
    '    # quantity    Quantity on order
    '    # total   Total value of order (price * quantity)
    Public Function marketOrders(marketid)
        Return api_query("marketorders", "{'marketid': marketid}")

    End Function
        



    ' # Inputs:
    ' # marketid    Market ID for which you are querying
    ' # limit   (optional) Limit the number of results. Default: 200
    ' ##
    ' # Outputs: Array your Trades for this Market, in Date Decending Order 
    ' # tradeid An integer identifier for this trade
    ' # tradetype   Type of trade (Buy/Sell)
    ' # datetime    Server datetime trade occurred
    ' # tradeprice  The price the trade occurred at
    ' # quantity    Quantity traded
    ' # total   Total value of trade (tradeprice * quantity)
    Public Function myTrades(marketid, limit)
        Return api_query("mytrades", "{'marketid': marketid, 'limit': limit}")

    End Function
        



    '    # Outputs: Array your Trades for all Markets, in Date Decending Order 
    '    # tradeid An integer identifier for this trade
    '    # tradetype   Type of trade (Buy/Sell)
    '    # datetime    Server datetime trade occurred
    '    # marketid    The market in which the trade occurred
    '    # tradeprice  The price the trade occurred at
    '    # quantity    Quantity traded
    '    # total   Total value of trade (tradeprice * quantity)
    Public Function allMyTrades()
        Return api_query("allmytrades", "")

    End Function
        



    '    # Inputs:
    '    # marketid    Market ID for which you are querying
    '    ##
    '    # Outputs: Array of your orders for this market listing your current open sell and buy orders. 
    '    # orderid Order ID for this order
    '    # created Datetime the order was created
    '    # ordertype   Type of order (Buy/Sell)
    '    # price   The price per unit for this order
    '    # quantity    Quantity for this order
    '    # total   Total value of order (price * quantity)
    Public Function myOrders(marketid)
        Return api_query("myorders", "{'marketid': marketid}")


    End Function
        



    '    # Inputs:
    '    # marketid    Market ID for which you are querying
    '    ##
    '    # Outputs: Array of buy and sell orders on the market representing market depth. 
    '    # Output Format is:
    '    # array(
    '    #   'sell'=>array(
    '    #     array(price,quantity), 
    '    #     array(price,quantity),
    '    #     ....
    '    #   ), 
    '    #   'buy'=>array(
    '    #     array(price,quantity), 
    '    #     array(price,quantity),
    '    #     ....
    '    #   )
    '    # )
    Public Function depth(marketid)
        Return api_query("depth", "{'marketid': marketid}")

    End Function
    



    '    # Outputs: Array of all open orders for your account. 
    '    # orderid Order ID for this order
    '    # marketid    The Market ID this order was created for
    '    # created Datetime the order was created
    '    # ordertype   Type of order (Buy/Sell)
    '    # price   The price per unit for this order
    '    # quantity    Quantity for this order
    '    # total   Total value of order (price * quantity)
    Public Function allMyOrders()
        Return api_query("allmyorders", "")

    End Function
        



    '    # Inputs:
    '    # marketid    Market ID for which you are creating an order for
    '    # ordertype   Order type you are creating (Buy/Sell)
    '    # quantity    Amount of units you are buying/selling in this order
    '    # price   Price per unit you are buying/selling at
    '    ##
    '    # Outputs: 
    '    # orderid If successful, the Order ID for the order which was created
    Public Function createOrder(self, marketid, ordertype, quantity, price)
        Return self.api_query("createorder", "{'marketid': marketid, 'ordertype': ordertype, 'quantity': quantity, 'price': price}")

    End Function
        


    '    # Inputs:
    '    # orderid Order ID for which you would like to cancel
    '    ##
    '    # Outputs: If successful, it will return a success code. 
    Public Function cancelOrder(orderid)
        Return api_query("cancelorder", "{'orderid': orderid}")
        '

    End Function
        


    '    # Inputs:
    '    # marketid    Market ID for which you would like to cancel all open orders
    '    ##
    '   # Outputs: 
    '  # return  Array for return information on each order cancelled
    Public Function cancelMarketOrders(marketid)

        Return api_query("cancelmarketorders", "{'marketid': marketid}")
    End Function



    '# Outputs: 
    '# return  Array for return information on each order cancelled
    Public Function cancelAllOrders()
        Return api_query("cancelallorders", "")


    End Function
        

    '    # Inputs:
    '    # ordertype   Order type you are calculating for (Buy/Sell)
    '    # quantity    Amount of units you are buying/selling
    '    # price   Price per unit you are buying/selling at
    '    ##
    '    # Outputs: 
    '    # fee The that would be charged for provided inputs
    '    # net The net total with fees
    Public Function calculateFees(ordertype, quantity, price)
        Return api_query("calculatefees", "{'ordertype': ordertype, 'quantity': quantity, 'price': price}")


    End Function




    '    # Inputs: (either currencyid OR currencycode required - you do not have to supply both)
    '    # currencyid  Currency ID for the coin you want to generate a new address for (ie. 3 = BitCoin)
    '    # currencycode    Currency Code for the coin you want to generate a new address for (ie. BTC = BitCoin)
    '    ##
    '    # Outputs: 
    '   # address The new generated address
    Public Function generateNewAddress(currencyid, currencycode)

        Dim req As String

        If currencyid <> "" Then
            req = "{'currencyid': " + currencyid + "}"
        ElseIf currencycode <> "" Then
            req = "{'currencycode': " + currencycode + "}"
        Else
            Return ""
        End If

        Return api_query("generatenewaddress", req)

    End Function
   

End Class

