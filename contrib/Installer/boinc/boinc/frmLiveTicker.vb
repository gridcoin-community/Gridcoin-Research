Imports System.Drawing
Imports System.Windows.Forms

Public Class frmLiveTicker
    Public lblTick(100) As Label
    Public lOffSet(100) As Long
    Dim sCarat(100) As String
    Public lMaxWidth As Long = 0
    Public AvgShown As Long = 0
    Public TotalShown As Long = 0
    Private LabelWidth As Long = 290
    Private LabelHeight As Long = 50
    Private mlHeightOffset As Long = 25

    Public Function NiceTicker(sSymbol As String)
        sSymbol = Replace(Replace(sSymbol, "$", ""), "^", "")
        Return sSymbol
    End Function
    Public Function DisplayTicker(sCarat As String, Price As Double)
        Dim sOut As String = NiceTicker(msCurrentSymbol) + " " + Trim(sCarat) + " " + Trim(Price)
        If Len(sOut) < 8 Then
            sOut += Space(8 - Len(sOut))
        End If
        Return sOut
    End Function
    Public Function RoomExists(ShownCount As Long)
        For x = 0 To mlStockCount - 1
            If lblTick(x).Left < LabelWidth And lblTick(x).Visible = True And lblTick(x).Text <> "" Then
                Return False
            End If
        Next
        Return True
    End Function
    Public Function IsHighest(z As Integer) As Boolean
        Dim hi As Long
        For x = 0 To mlStockCount - 1
            If lblTick(x).Left > Me.Width And lblTick(x).Visible = True And lblTick(x).Text <> "" And lblTick(x).Left > hi Then
                hi = lblTick(x).Left
            End If
        Next
        If lblTick(z).Left = hi Then Return True Else Return False
    End Function
    Private Sub Timer1_Tick(sender As System.Object, e As System.EventArgs) Handles Timer1.Tick
        Dim dLeftPos As Long = 0
        For x = 0 To mlStockCount - 1
            msCurrentSymbol = mvTickers(x)
            Dim CurrentQuote As Quote
            CurrentQuote = GetQuote(msCurrentSymbol)
            If CurrentQuote.Price > 0 Then
                lOffSet(x) = lOffSet(x) + 1
                lblTick(x).Text = msCurrentSymbol
                Dim lCurrPos As Long = lOffSet(x) + dLeftPos
                lblTick(x).Left = lCurrPos
                If (lCurrPos > Me.lMaxWidth) Then lMaxWidth = lCurrPos
                If lCurrPos > Me.Width And RoomExists(CurrentQuote.ShownCount) And IsHighest(x) Then
                    'Reset
                    lOffSet(x) = 0 - dLeftPos
                    If CurrentQuote.IsDown Then
                        lblTick(x).ForeColor = Color.Red
                        sCarat(x) = "-"
                    Else
                        lblTick(x).ForeColor = Color.Lime
                        sCarat(x) = "+"
                    End If
                    lblTick(x).Visible = True
                    CurrentQuote.ShownCount += 1
                    mdPrices(msCurrentSymbol) = CurrentQuote
                    TotalShown = TotalShown + 1
                    AvgShown = TotalShown / mlStockCount
                End If
                lblTick(x).Height = LabelHeight
                lblTick(x).Text = DisplayTicker(sCarat(x), CurrentQuote.Price)
                lblTick(x).Width = LabelWidth
                dLeftPos = dLeftPos + lblTick(x).Width + 0
                lblTick(x).Top = Me.Height - lblTick(x).Height - mlHeightOffset

            End If
        Next x
    End Sub
    Public Function GetCryptoPrice(sSymbol As String)
        Try
            Dim sSymbol1 As String
            sSymbol1 = NiceTicker(sSymbol)
            Dim ccxPage As String = ""
            If sSymbol1 = "BTC" Then
                ccxPage = "btc-usd.json"
            Else
                ccxPage = LCase(sSymbol1) + "-btc.json"
            End If
            Dim sURL As String = "https://c-cex.com/t/" + ccxPage
            Dim w As New MyWebClient
            Dim sJSON As String = w.DownloadString(sURL)
            Dim sLast As String = ExtractValue(sJSON, "lastprice")
            sLast = Replace(sLast, ",", ".")
            Dim dprice As Double
            dprice = CDbl(sLast)
            Dim qBitcoin As Quote
            qBitcoin = GetQuote("$BTC")
            If sSymbol1 <> "BTC" And qBitcoin.Price > 0 Then dprice = dprice * qBitcoin.Price
            Dim q As Quote
            q = GetQuote(sSymbol)
            q.Symbol = sSymbol
            q.PreviousClose = q.Price
            Dim Variance As Double
            Variance = Math.Round(dprice, 3) - q.PreviousClose
            q.Variance = Variance
            q.Price = Math.Round(dprice, 3)
            mdPrices(sSymbol) = q
        Catch ex As Exception
            Log("Unable to get quote data probably due to SSL being blocked: " + ex.Message)

        End Try

    End Function
    Private Sub Form1_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load

        Dim lHO = Val("0" & KeyValue("TickerHeightOffset"))
        If lHO > 0 Then mlHeightOffset = lHO

        'Note: ^DJI does not work for some reason... Use ^DJX*10 for index price
        'Example = msTickers = "GOOG,^DJX,^RUT,^VIX,SPY,AAPL,$GRC,$BTC,SHLD,$LTC"
        msTickers = KeyValue("tickers")
        If Len(msTickers) = 0 Then
            UpdateKey("tickers", "GOOG,^DJX,^RUT,^VIX,SPY,AAPL,$GRC,$BTC,SHLD,$LTC")
            msTickers = KeyValue("tickers")
        End If
        mvTickers = Split(msTickers, ",")
        mlStockCount = UBound(mvTickers) + 1
        For x = 0 To mlStockCount - 1
            lblTick(x) = New Label
            lblTick(x).BackColor = Color.Black
            lblTick(x).ForeColor = Color.Green
            lblTick(x).Font = New Font("Arial", "24")
            lblTick(x).Visible = True
            msCurrentSymbol = mvTickers(x)
            Me.Controls.Add(lblTick(x))
        Next
        GetCryptoPrice("$BTC")
        Dim th As New System.Threading.Thread(AddressOf GrabPrices)
        th.Start()
    End Sub
    Public Sub GrabPrices()
        For x = 1 To mlStockCount
            Call GetPrice(mvTickers(x - 1))
        Next
        Call MegaQuote()
    End Sub
    Public Function GetPrice(sSymbol As String)
        If sSymbol.StartsWith("$") Then
            GetCryptoPrice(sSymbol)
        Else
            'Reserved to get a single equity price
        End If
    End Function
    Private Sub Timer2_Tick(sender As System.Object, e As System.EventArgs) Handles Timer2.Tick
        Dim th As New System.Threading.Thread(AddressOf GrabPrices)
        th.Start()
    End Sub
    Friend WithEvents Timer1 As System.Windows.Forms.Timer
    Friend WithEvents Timer2 As System.Windows.Forms.Timer
    'Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()> _
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
        Finally
            MyBase.Dispose(disposing)
        End Try
    End Sub

    'Required by the Windows Form Designer
    Private components As System.ComponentModel.IContainer

    Private Sub InitializeComponent()
        Me.components = New System.ComponentModel.Container()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmLiveTicker))
        Me.Timer1 = New System.Windows.Forms.Timer(Me.components)
        Me.Timer2 = New System.Windows.Forms.Timer(Me.components)
        Me.SuspendLayout()
        '
        'Timer1
        '
        Me.Timer1.Enabled = True
        Me.Timer1.Interval = 2
        '
        'Timer2
        '
        Me.Timer2.Interval = 240000
        '
        'frmLiveTicker
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(1461, 122)
        Me.DoubleBuffered = True
        Me.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmLiveTicker"
        Me.Text = "Gridcoin Live Ticker"
        Me.ResumeLayout(False)

    End Sub
End Class
