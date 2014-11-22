
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

            Dim Port As String = "8080"

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

                strRequest = rootPath & strRequest

                sendHTMLResponse(strRequest)

            Else ' Not HTTP GET method
                strRequest = rootPath & "Error\" & "400.html"

                sendHTMLResponse(strRequest)
            End If

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