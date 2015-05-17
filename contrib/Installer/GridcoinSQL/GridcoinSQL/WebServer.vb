
Imports System.Net
Imports System.Net.Sockets
Imports System.Threading
Imports System.Text
Imports System.IO
Public Class WebServer

    Sub New()

        Try
            Dim hostName As String = Dns.GetHostName()
            Dim serverIP As IPAddress = Dns.Resolve(hostName).AddressList(0)

            Dim Port As String = "32500"

            Dim tcpListener As New TcpListener(serverIP, Int32.Parse(Port))

            tcpListener.Start()

            Console.WriteLine("Web server started at: " & serverIP.ToString() & ":" & Port)

            Dim httpSession As New HTTPSession(tcpListener)

            Dim serverThread As New Thread(New ThreadStart(AddressOf httpSession.ProcessThread))

            serverThread.Start()

        Catch ex As Exception
            Console.WriteLine(ex.StackTrace.ToString())
        End Try
    End Sub
End Class




Public Class HTTPSession
    Private tcpListener As System.Net.Sockets.TcpListener
    Private clientSocket As System.Net.Sockets.Socket
    Dim DictQS As New Dictionary(Of String, String)

    Public Sub New(ByVal tcpListener As System.Net.Sockets.TcpListener)
        Me.tcpListener = tcpListener
    End Sub

    Public Sub ProcessThread()
        While (True)
            Try
                clientSocket = tcpListener.AcceptSocket()

                ' Socket Information
                Dim clientInfo As IPEndPoint = CType(clientSocket.RemoteEndPoint, IPEndPoint)

                Console.WriteLine("Client: " + clientInfo.Address.ToString() + ":" + clientInfo.Port.ToString())

                ' Set Thread for each Web Browser Connection
                Dim clientThread As New Thread(New ThreadStart(AddressOf ProcessRequest))

                clientThread.Start()

            Catch ex As Exception
                Console.WriteLine(ex.StackTrace.ToString())

                If clientSocket.Connected Then
                    clientSocket.Close()
                End If

            End Try
        End While
    End Sub
    Private Sub AddNode(sNode As String)
        Try

            Dim oSql As New SQLBase("gridcoinsql")
            Dim sData As String
            oSql.InsertRecord("node", "Host,LastSeen", "'" + Trim(sNode) + "',getdate()")
        Catch ex As Exception

        End Try
    End Sub

    Public Function ExtractXML(sData As String, sStartKey As String, sEndKey As String)
        Dim iPos1 As Integer = InStr(1, sData, sStartKey)
        iPos1 = iPos1 + Len(sStartKey)
        Dim iPos2 As Integer = InStr(iPos1, sData, sEndKey)
        iPos2 = iPos2
        If iPos2 = 0 Then Return ""

        Dim sOut As String = Mid(sData, iPos1, iPos2 - iPos1)
        Return sOut

    End Function

    Public Function DetermineHtmlRequest(htmlReq As String)
        ' Set default page
        Dim defaultPage As String = "index.html"

        Dim strArray() As String
        Dim strRequest As String

        strArray = htmlReq.Trim.Split(" ")

        ' Determine the HTTP method (GET only)
        If strArray(0).Trim().ToUpper.Equals("GET") Then
            strRequest = strArray(1).Trim

            If (strRequest.StartsWith("/")) Then
                strRequest = strRequest.Substring(1)
            End If

            If (strRequest.EndsWith("/") Or strRequest.Equals("")) Then
                strRequest = strRequest & defaultPage
            End If

            If InStr(1, strRequest, "?") > 0 Then

                Dim sQS() As String
                sQS = Split(strRequest, "?")
                Dim qs1 As String
                qs1 = sQS(1)
                Dim qs2() As String
                Dim QSDecoded As String

                QSDecoded = Replace(qs1, "%20", " ")

                qs2 = Split(QSDecoded, "&")
                Dim p1 As String
                Dim p2 As String
                Dim p3 As String



                For x = 0 To qs2.Length - 1

                    Dim vRow() As String
                    vRow = Split(qs2(x), "=")
                    p1 = vRow(0)
                    p2 = vRow(1)
                    DictQS.Remove(p1)

                    DictQS.Add(p1, p2)
                Next

                '      Dim clientInfo As IPEndPoint = CType(clientSocket.RemoteEndPoint, IPEndPoint)
                '     Console.WriteLine("Client: " + clientInfo.Address.ToString() + ":" + clientInfo.Port.ToString())
                '    Dim sClient As String
                '   sClient = clientInfo.Address.ToString() + ":" + clientInfo.Port.ToString()
                'Insert the node into Nodes
                ' AddNode(sClient)

            End If
        End If

        Return strRequest

    End Function

    Protected Sub ProcessRequest()
        Dim recvBytes(1024) As Byte
        Dim htmlReq As String = Nothing
        Dim bytes As Int32

        Try
            ' Receive HTTP Request from Web Browser
            bytes = clientSocket.Receive(recvBytes, 0, clientSocket.Available, SocketFlags.None)
            htmlReq = Encoding.ASCII.GetString(recvBytes, 0, bytes)

            Console.WriteLine("HTTP Request: ")
            Console.WriteLine(htmlReq)

            ' Set WWW Root Path
            Dim rootPath As String = Directory.GetCurrentDirectory() & "\WWWRoot\"

          
            Dim strRequest As String
            Dim sSQL As String
            sSQL = ExtractXML(htmlReq, "<QUERY>", "</QUERY>")

            Debug.Print(sSQL)

            Dim sFromNode As String
            sFromNode = ExtractXML(htmlReq, "<FROMNODE>", "</FROMNODE>")


            Dim clientInfo As IPEndPoint = CType(clientSocket.RemoteEndPoint, IPEndPoint)
            Dim sClient As String
            sClient = clientInfo.Address.ToString() + ":" + clientInfo.Port.ToString()
            'Insert the node into Nodes
            AddNode(sClient)

            Dim sData As String
            If Len(sSQL) > 0 Then

                Try
                    sData = GetHttpData(sSQL)

                Catch ex As Exception
                    sData = ex.Message

                End Try
                strRequest = DetermineHtmlRequest(htmlReq)

                sendHTMLResponseFromData(strRequest, sData)


                Exit Sub
            End If

            If htmlReq = "" Then Exit Sub


                    sData = GetHttpData(DictQS("table"), DictQS("startdate"), DictQS("enddate"))

                    sendHTMLResponseFromData(strRequest, sData)


                    strRequest = rootPath & strRequest

                    sendHTMLResponse(strRequest)
                    Stop
            

            'Else ' Not HTTP GET method
                strRequest = rootPath & "Error\" & "400.html"

                sendHTMLResponse(strRequest)
           
        Catch ex As Exception
            
            Console.WriteLine(ex.StackTrace.ToString())

            If clientSocket.Connected Then
                clientSocket.Close()
            End If
        End Try
    End Sub

    Public Function GetHttpData(sTable As String, sStart As String, sEnd As String)
        Dim oSql As SQLBase

        Try
            oSql = New SQLBase("gridcoinsql")

        Catch ex As Exception
            Debug.Print(ex.Message)

        End Try
        Dim sData As String
        Dim sInsert As String

        sData = oSql.TableToData(sTable, sStart, sEnd)

        Return sData


    End Function
    Public Function GetHttpData(sSql As String)
        'Main SQL Receiver
        Dim oSql As New SQLBase("gridcoinsql")
        Dim sData As String
        Dim sInsert As String
        sInsert = ExtractXML(sSql, "<INSERT>", "</INSERT>")
        Dim sUpdate As String = ExtractXML(sSql, "<UPDATE>", "</UPDATE>")

        If Len(sInsert) > 0 Then
            Dim sTable As String = ExtractXML(sInsert, "<TABLE>", "</TABLE>")
            Dim sFields As String = ExtractXML(sInsert, "<FIELDS>", "</FIELDS>")
            Dim sValues As String = ExtractXML(sInsert, "<VALUES>", "</VALUES>")
            Dim sResult As String = oSql.InsertRecord(sTable, sFields, sValues)
            Return sResult


            Stop
        ElseIf sSql = "SELECT * FROM INTERNALTABLES" Then
            Return oSql.GetInternalTables()
        ElseIf Len(sUpdate) > 0 Then
            Dim sTable As String = ExtractXML(sUpdate, "<TABLE>", "</TABLE>")
            Dim sFields As String = ExtractXML(sUpdate, "<FIELDS>", "</FIELDS>")
            Dim sValues As String = ExtractXML(sUpdate, "<VALUES>", "</VALUES>")
            Dim sWhereFields As String = ExtractXML(sUpdate, "<WHEREFIELDS>", "</WHEREFIELDS>")
            Dim sWhereValues As String = ExtractXML(sUpdate, "<WHEREVALUES>", "</WHEREVALUES>")

            Dim sResult As String = oSql.UpdateRecord(sTable, sFields, sValues, sWhereFields, sWhereValues)

            Return sResult

        End If



        sData = oSql.SqlToData(sSql)

        Return sData


    End Function



    ' Send HTTP Response
    Private Sub sendHTMLResponseFromData(ByVal httpRequest As String, sData As String)

        Try
            ' Get the file content of HTTP Request 
            '    Dim streamReader As StreamReader = New StreamReader(httpRequest)
            '   Dim strBuff As String = streamReader.ReadToEnd()
            '  streamReader.Close()
            ' streamReader = Nothing

            ' The content Length of HTTP Request
            Dim respByte() As Byte = Encoding.ASCII.GetBytes(sData)


            ' Set HTML Header
            Dim htmlHeader As String = _
                "HTTP/1.0 200 OK" & ControlChars.CrLf & _
                "Server: WebServer 1.0" & ControlChars.CrLf & _
                "Content-Length: " & respByte.Length & ControlChars.CrLf & _
                "Content-Type: " & getContentType(httpRequest) & _
                ControlChars.CrLf & ControlChars.CrLf

            ' The content Length of HTML Header
            Dim headerByte() As Byte = Encoding.ASCII.GetBytes(htmlHeader)

            Console.WriteLine("HTML Header: " & ControlChars.CrLf & htmlHeader)

            ' Send HTML Header back to Web Browser
            clientSocket.Send(headerByte, 0, headerByte.Length, SocketFlags.None)

            ' Send HTML Content back to Web Browser
            clientSocket.Send(respByte, 0, respByte.Length, SocketFlags.None)

            ' Close HTTP Socket connection
            clientSocket.Shutdown(SocketShutdown.Both)
            clientSocket.Close()

        Catch ex As Exception
            Console.WriteLine(ex.StackTrace.ToString())

            If clientSocket.Connected Then
                clientSocket.Close()
            End If
        End Try
    End Sub




    ' Send HTTP Response
    Private Sub sendHTMLResponse(ByVal httpRequest As String)
        Try
            ' Get the file content of HTTP Request 
            Dim streamReader As StreamReader = New StreamReader(httpRequest)
            Dim strBuff As String = streamReader.ReadToEnd()
            streamReader.Close()
            streamReader = Nothing

            ' The content Length of HTTP Request
            Dim respByte() As Byte = Encoding.ASCII.GetBytes(strBuff)

            ' Set HTML Header
            Dim htmlHeader As String = _
                "HTTP/1.0 200 OK" & ControlChars.CrLf & _
                "Server: WebServer 1.0" & ControlChars.CrLf & _
                "Content-Length: " & respByte.Length & ControlChars.CrLf & _
                "Content-Type: " & getContentType(httpRequest) & _
                ControlChars.CrLf & ControlChars.CrLf

            ' The content Length of HTML Header
            Dim headerByte() As Byte = Encoding.ASCII.GetBytes(htmlHeader)

            Console.WriteLine("HTML Header: " & ControlChars.CrLf & htmlHeader)

            ' Send HTML Header back to Web Browser
            clientSocket.Send(headerByte, 0, headerByte.Length, SocketFlags.None)

            ' Send HTML Content back to Web Browser
            clientSocket.Send(respByte, 0, respByte.Length, SocketFlags.None)

            ' Close HTTP Socket connection
            clientSocket.Shutdown(SocketShutdown.Both)
            clientSocket.Close()

        Catch ex As Exception
            Console.WriteLine(ex.StackTrace.ToString())

            If clientSocket.Connected Then
                clientSocket.Close()
            End If
        End Try
    End Sub

    ' Get Content Type
    Private Function getContentType(ByVal httpRequest As String) As String
        If (httpRequest.EndsWith("html")) Then
            Return "text/html"
        ElseIf (httpRequest.EndsWith("htm")) Then
            Return "text/html"
        ElseIf (httpRequest.EndsWith("txt")) Then
            Return "text/plain"
        ElseIf (httpRequest.EndsWith("gif")) Then
            Return "image/gif"
        ElseIf (httpRequest.EndsWith("jpg")) Then
            Return "image/jpeg"
        ElseIf (httpRequest.EndsWith("jpeg")) Then
            Return "image/jpeg"
        ElseIf (httpRequest.EndsWith("pdf")) Then
            Return "application/pdf"
        ElseIf (httpRequest.EndsWith("pdf")) Then
            Return "application/pdf"
        ElseIf (httpRequest.EndsWith("doc")) Then
            Return "application/msword"
        ElseIf (httpRequest.EndsWith("xls")) Then
            Return "application/vnd.ms-excel"
        ElseIf (httpRequest.EndsWith("ppt")) Then
            Return "application/vnd.ms-powerpoint"
        Else
            Return "text/plain"
        End If
    End Function
End Class