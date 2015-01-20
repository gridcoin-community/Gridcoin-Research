
Imports BoincStake

Imports System.Net.HttpWebRequest
Imports System.Text
Imports System.IO
Imports System.Data
Imports System.Object
Imports System.Security.Cryptography
Imports System.Speech.Synthesis


Imports System.Net

Public Class Form1

    'Public m As New Utilization

    Public night As Boolean


    Public Structure Crypt
        Dim Success As Double
        Dim [Return] As Object

    End Structure
    Public Structure Symbol
        Public marketid As String
        Public label As String
        Public lasttradeprice As Double
        Public volume As Double
        Public lasttradetime As String
        Public primaryname As String
        Public primarycode As String
        Public secondaryname As String
        Public secondarycode As String
        Public rencenttrades As Object
    End Structure
    Public Function ByteToString(b() As Byte)
        Dim sReq As String
        sReq = System.Text.Encoding.UTF8.GetString(b)
        Return sReq
    End Function
    Public Function RetrieveSiteSecurityInformation(sURL As String) As String
        Dim u As New Uri(sURL)
        Dim sp As ServicePoint = ServicePointManager.FindServicePoint(u)
        Dim groupName As String = Guid.NewGuid().ToString()
        Dim req As HttpWebRequest = TryCast(HttpWebRequest.Create(u), HttpWebRequest)
        req.ConnectionGroupName = groupName
        Try

            Using resp As WebResponse = req.GetResponse()
            End Using
            sp.CloseConnectionGroup(groupName)
            Dim key As Byte() = sp.Certificate.GetPublicKey()
            Dim sOut As String
            sOut = ByteArrayToHexString(key)
            Return sOut
        Catch ex As Exception
            'Usually due to either HTTP, 501, Not Implemented...etc.
            Return ""
        End Try

    End Function
    Public Function ValidatePoolURL(sURL As String) As Boolean
        Dim Pools(100) As String
        Pools(1) = "gridcoin.us,30 82 01 0a 02 82 01 01 00 e1 91 3f 65 da 2b cc de 81 10 be 21 bd 8a 22 00 c5 8d 5f d6 72 5d 1c 3c e4 0b 3a 03 c8 07 c1 e1 69 54 22 d3 ff 9e d7 55 55 c2 2e 62 bd 5c bc f5 3f 93 3d f1 2c 39 0b 66 04 a8 50 7e f5 19 ca 97 a5 99 02 0b 11 39 37 5e df a2 74 14 f1 ed be eb af 4b 53 c2 cc a9 ea 5f c0 0a cb 92 cf 7f 21 fc 96 4f 79 47 e9 15 97 58 65 ef 10 a3 3e 46 6a 1d 5b 34 ea ff 6d c6 10 08 b8 60 dd 40 d5 b3 43 73 96 70 9f ce f1 2c 3b 8e 09 e0 14 97 9e b3 c6 6c a2 d9 81 4d d4 71 f1 46 ae ec b9 cf 0b 59 bd 7a 85 88 48 0f aa fa 6e f5 1a 75 18 f0 c9 94 79 6c 8b 11 86 de 3f ab 76 62 77 99 5a c4 fb 10 79 35 3d 61 33 15 ed a8 0c ce 45 cd 3e fc 64 62 72 07 a2 05 b4 df 3c f8 97 7c f9 20 43 b6 93 c2 2a 67 b7 9c 64 36 2f 9f 2d c3 d1 82 1a 9c 85 bb 3f d6 b7 07 aa 23 a3 a9 6a 49 18 f1 46 b5 b3 11 b6 61 02 03 01 00 01"

        ''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''Verify SSL Certificate'''''''''''''''''''''''''''
        Dim sHexSSLPublicKey As String = RetrieveSiteSecurityInformation(sURL)
        If Len(sHexSSLPublicKey) = 0 Then Return False

        For x As Integer = 0 To 100
            If Len(Pools(x)) > 0 Then
                Dim vPools() As String = Split(Pools(x), ",")
                If UBound(vPools) = 1 Then
                    Dim sPoolPublicKey As String = vPools(1)
                    If Len(sPoolPublicKey) > 0 Then
                        sPoolPublicKey = Replace(sPoolPublicKey, " ", "")
                        If LCase(sPoolPublicKey) = LCase(sHexSSLPublicKey) Then
                            Return True
                        End If
                    End If
                End If
            End If
        Next
        Return False

    End Function

    Public Function ByteArrayToHexString(ByVal ba As Byte()) As String
        Dim hex As StringBuilder
        hex = New StringBuilder(ba.Length * 2)
        For Each b As Byte In ba
            hex.AppendFormat("{0:x2}", b)
        Next
        Return hex.ToString()
    End Function


    Public bu As New BoincStake.Utilization

    Public Function ParseDate(sDate As String)
        'parses microsofts IIS date to a date, globally
        Dim vDate() As String
        vDate = Split(sDate, " ")
        If UBound(vDate) > 0 Then
            Dim sEle1 As String = vDate(0)
            Dim vEle() As String
            vEle = Split(sEle1, "/")
            If UBound(vEle) > 1 Then
                'Handle mm-dd-yyyy or dd-mm-yyyy or web server PST or web server UST:
                Dim sTr As String
                Dim sTime1 As String
                Dim sTime2 As String
                sTime1 = vDate(UBound(vDate) - 1)
                sTime2 = vDate(UBound(vDate) - 0)
                sTr = Trim(DateSerial(vEle(2), vEle(0), vEle(1))) + " " + Trim(sTime1) + " " + Trim(sTime2)
                If IsDate(sTr) Then Return CDate(sTr) Else Return CDate("1-1-2031")
            End If
        End If
        Return CDate("1-1-2031")

    End Function
    Private Sub Button1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button1.Click
        Dim x As New Utilization
        x.SetDebugMode(False)
        x.ShowSql()
        x.ShowTicketList()

    End Sub


    Public Sub RewriteSourceFile(sPATHH As String, sSFileIn As String)
        Dim sFileIn As String
        sFileIn = sPATHH + sSFileIn

        Dim infile As New StreamReader(sFileIn)
        Dim sOutPath As String
        Dim di As New DirectoryInfo(sFileIn)

        If night Then
            sOutPath = Replace(di.FullName, "-master", "-black")


        Else
            sOutPath = Replace(di.FullName, "-master", "-white")

        End If

        Dim outfile As New StreamWriter(sOutPath)
        Dim text As String
        Dim sBackupPath As String = "c:\gridcoin-master\src\qcolorbackup\"
        Dim sFileName As String
        Dim fi As New FileInfo(sFileIn)
        FileCopy(sFileIn, sBackupPath + fi.Name)
        While (infile.EndOfStream = False)
            text = infile.ReadLine()
            If InStr(1, text, "<color alpha=") > 0 Then
                text = text + vbCrLf + infile.ReadLine + vbCrLf + infile.ReadLine + vbCrLf + infile.ReadLine
                '<red>0</red>
                '        <green>255</green>
                '        <blue>0</blue>
                text = ReplaceRGB(text, night)
                Stop

            End If
            'BLACK
            text = Replace(text, "QColor(25,25,25)", IIf(night, "QColor(0,0,0)", "QColor(255,255,255)")) 'Black to black or white

            text = Replace(text, "background-color: #08e8e8", IIf(night, "background-color: #0817E8", "background-color: #0817FF")) 'progress bar blue or blue

            '//QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
            text = Replace(text, "#FF8000", IIf(night, "#44FF00", "#44FF00"))  'instead of orange, green or green
            text = Replace(text, "color: red", IIf(night, "color: #FF0000", "color: #FF0000"))
            text = Replace(text, "QColor(200, 0, 0)", IIf(night, "QColor(255, 0, 0)", "QColor(255, 0, 0)"))  'red
            'pink = c80075 or 200,0,117
            text = Replace(text, "color: #808080", IIf(night, "color: #808080", "color: #8080")) ' gray
            text = Replace(text, "color: #006060", IIf(night, "color: #00ff00", "color: #00ff00"))  'light green instead of blue
            text = Replace(text, "QColor(210,200,180)", IIf(night, "QColor(145,158,152)", "QColor(204,230,232")) '  light blue almost white
            text = Replace(text, "QColor(200,50,50)", IIf(night, "QColor(255,0,0)", "QColor(255,0,0")) '  Tx Table Model (red) going to Red
            text = Replace(text, "QColor(200,0,0)", IIf(night, "QColor(255,0,0)", "QColor(255,0,0)")) '  Tx Table Model (red) going to Red
            text = Replace(text, "QColor(200, 0, 0)", IIf(night, "QColor(255,0,0)", "QColor(255,0,0")) '  Tx Table Model (red) going to Red
            text = Replace(text, "QColor(200,55,0)", IIf(night, "QColor(255,0,0)", "QColor(255,0,0")) '  Tx Table Model (BURNT ORANGE) going to Red
            text = Replace(text, "#161616", IIf(night, "#000000", "#161616")) '  Tx Table Model (dark black) going to black 
            text = Replace(text, "#363636", IIf(night, "#363636", "#525252")) '  Tx Table Model (lighter row alt black) going to 
            text = Replace(text, "#121212", IIf(night, "#000000", "#161616")) '  Tx Table Model (dark black) going to very dark green
            text = Replace(text, "QColor(0,240,40)", IIf(night, "QColor(0,255,0)", "QColor(0,255,0"))  '  guiconstants.h goinf from green to light green
            text = Replace(text, "QColor(0, 255,55)", IIf(night, "QColor(0,255,0)", "QColor(0,255,0")) '  guiconstants.h goinf from green to light green
            outfile.WriteLine(text)

        End While
        outfile.Close()

    End Sub
    Public Function ReplaceRGB(sData As String, night As Boolean)

        '<red>0</red>
        '        <green>255</green>
        '        <blue>0</blue>
        Dim r As Long
        Dim g As Long
        Dim b As Long
        Dim sOut As String

        GetRgb(sData, r, g, b)
        If r = 0 And g = 255 And b = 0 Then
            sOut = IIf(night, WriteRgb(0, 255, 0), WriteRgb(0, 255, 0)) ' going from green to green, green
            Return sOut
        End If

        If r = 27 And g = 240 And b = 134 Then
            sOut = IIf(night, WriteRgb(0, 255, 0), WriteRgb(0, 255, 0)) 'dark green to green
            Return sOut
        End If
        If r = 137 And g = 148 And b = 68 Then
            sOut = IIf(night, WriteRgb(0, 255, 0), WriteRgb(0, 255, 0)) 'tan      to green
            Return sOut
        End If

        If r = 255 And g = 255 And b = 255 Then
            sOut = IIf(night, WriteRgb(255, 255, 255), WriteRgb(0, 0, 0)) 'white to white or black
            Return sOut

        End If
        If r = 0 And g = 0 And b = 0 Then
            sOut = IIf(night, WriteRgb(0, 0, 0), WriteRgb(255, 255, 255)) 'black to white
            Return sOut

        End If
        'green to light green 
        If IsColor(27, 240, 134, r, g, b, 0, 255, 0, 0, 255, 0, night, sOut) Then Return sOut

        'Menu Button Alt Fore Color (Dark blue 0 0 127) 

        If IsColor(0, 0, 127, r, g, b, 0, 0, 127, 0, 0, 127, night, sOut) Then Return sOut
        If IsColor(137, 148, 68, r, g, b, 0, 255, 0, 0, 255, 0, night, sOut) Then Return sOut 'UPS tan (137-148-68)
        If IsColor(120, 120, 120, r, g, b, 120, 120, 120, 120, 120, 120, night, sOut) Then Return sOut ' med gray
        If IsColor(46, 57, 1, r, g, b, 255, 255, 255, 0, 0, 0, night, sOut) Then Return sOut ' very dark olive to black



        sOut = WriteRgb(r, g, b)
        Return sOut


    End Function
    Public Function IsColor(TestR As Long, TestG As Long, TestB As Long, _
                            sourceR As Long, SourceG As Long, sourceB As Long, _
                            nightr As Long, nightg As Long, nightb As Long, dayr As Long, dayg As Long, _
                            dayb As Long, night As Boolean, ByRef sOut As String) As Boolean

        If TestR = sourceR And TestG = SourceG And TestB = sourceB Then
            sOut = IIf(night, WriteRgb(nightr, nightg, nightb), WriteRgb(dayr, dayg, dayb))
            Return True
        End If

    End Function
    Public Function GetRgb(sdata As String, ByRef r As Long, ByRef g As Long, ByRef b As Long)

        Dim vData() As String
        vData = Split(sdata, vbCrLf)
        For x = 0 To UBound(vData)
            Dim sTemp As String = vData(x)
            If InStr(1, sTemp, "<red>") > 0 Then
                sTemp = Replace(sTemp, "<red>", "")
                sTemp = Replace(sTemp, "</red>", "")
                r = Val(sTemp)

            End If
            If InStr(1, sTemp, "<green>") > 0 Then
                sTemp = Replace(sTemp, "<green>", "")
                sTemp = Replace(sTemp, "</green>", "")
                g = Val(sTemp)
            End If
            If InStr(1, sTemp, "<blue>") > 0 Then
                sTemp = Replace(sTemp, "<blue>", "")
                sTemp = Replace(sTemp, "</blue>", "")
                b = Val(sTemp)
            End If
        Next x
    End Function

    Public Function WriteRgb(r As Long, g As Long, b As Long) As String
        Dim sOut As String
        sOut = "<color alpha=""255"">" + vbCrLf + "<red>" + Trim(r) + "</red>" + vbCrLf + "<green>" + Trim(g) + "</green>" + vbCrLf + "<blue>" + Trim(b) + "</blue>"
        Return sOut
    End Function

    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = "c:\cryptsy.txt"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(Now) + ", " + sData)
            sw.Close()
        Catch ex As Exception
        End Try

    End Sub

    Private Sub Button2_Click(sender As System.Object, e As System.EventArgs) Handles Button2.Click
        bu.StopWireFrameRenderer()

    End Sub
End Class
