Public Module modGridcoinTicker

    Public msTickers As String = ""
    Public mvTickers() As String
    Public msCurrentSymbol As String = ""
    Public mdPrices As New Dictionary(Of String, Quote)
    Public mlStockCount As Long = 0

    Public Structure Quote
        Dim Symbol As String
        Dim Price As Double
        Dim Variance As Double
        Dim PreviousClose As Double
        Dim ShownCount As Double

        Public Function IsDown() As Boolean
            Dim bDown As Boolean = False
            If Variance < 0 Then Return True Else Return False
        End Function
    End Structure

    Public Class MyWebClient
        Inherits System.Net.WebClient
        Private timeout As Long = 10000
        Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
            Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
            w.Timeout = timeout
            Return (w)
        End Function
    End Class
    Public Function GetQuote(sSymbol As String) As Quote
        Dim q As Quote
        If mdPrices.ContainsKey(sSymbol) = False Then
            q = New Quote
            mdPrices.Add(sSymbol, q)
        Else
            q = mdPrices(sSymbol)
        End If
        Return q
    End Function
    Public Function ExtractValue(sData As String, sKey As String)
        Dim iPos1 As Integer = InStr(1, sData, sKey)
        iPos1 = iPos1 + Len(sKey)
        Dim iPos2 As Integer = InStr(iPos1, sData, ",")
        'Incase the location of sKey moves to end of the json -- This similar scenario broke the quotes
        If iPos2 = 0 Then iPos2 = InStr(iPos1, sData, "}")
        'If 0 then return nothing
        If iPos2 = 0 Then Return ""
        Dim sOut As String = Mid(sData, iPos1, iPos2 - iPos1)
        sOut = Replace(sOut, Chr(34), "")
        sOut = Replace(sOut, Chr(58), "")
        Return sOut
    End Function
    Public Sub MegaQuote()
        Try
            'Gets all the equity prices in one request for efficiency...
            Dim sYTickers As String = ""
            sYTickers = Replace(msTickers, ",", "+")
            Dim sURL As String = ""
            sURL = "http://download.finance.yahoo.com/d/quotes.csv?s=" + sYTickers + "&f=snl1c1p2&e=.csv"
            Dim wc As New MyWebClient
            Dim sOut As String = wc.DownloadString(sURL)
            Dim vTickers() As String
            vTickers = Split(msTickers, ",")
            Dim vQuotes() As String
            vQuotes = Split(sOut, Chr(10))

            Dim dOut As Double
            Dim x As Long = 0
            For x = 0 To UBound(vTickers)
                dOut = MegaOrdinal(vTickers(x), vQuotes)
            Next

        Catch ex As Exception
            Log("Unable to get quote data probably due to SSL being blocked: " + ex.Message)

        End Try

    End Sub
    Public Function MegaOrdinal(sTicker As String, vData() As String) As Double
        For x = 0 To UBound(vData)
            Dim vRow() As String
            Dim sSeed As String = ""
            sSeed = Replace(vData(x), Chr(34), "")
            sSeed = Replace(sSeed, Chr(10), "")
            vRow = Split(sSeed, ",")
            Dim sSymbol As String = vRow(0)
            If Trim(sSymbol) = Trim(sTicker) Then
                Dim dPrice As Double = 0
                dPrice = Val(vRow(2))
                Dim q As Quote
                q = GetQuote(sSymbol)
                q.Price = Math.Round(dPrice, 2)
                'q.PreviousClose = Val(vRow(?))
                q.Variance = Val(vRow(3))
                mdPrices(sSymbol) = q

                Return dPrice
            End If
        Next
        Return 0
    End Function
End Module
