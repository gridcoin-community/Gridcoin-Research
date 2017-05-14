Imports System.IO
Imports System.Net.Sockets
Imports System.Globalization

Public Class GRCDiagnostics

    Public Function GetNISTDateTime() As DateTime
        Try

            Dim sNISTHOST As String = "pool.ntp.org" 'Atomic Clock Time Server in UTC
            Dim client = New TcpClient(sNISTHOST, 13)
            Dim localDateTime As DateTime

            Using streamReader = New StreamReader(client.GetStream())
                Dim response = streamReader.ReadToEnd()
                Dim utcDateTimeString = response.Substring(7, 17)
                '57551 16-06-12 16:57:53 50 0 0 353.8 UTC(NIST)
                Dim sChopped As String = Mid(response, 7, 100)
                sChopped = Replace(sChopped, "(NIST) *", "")
                sChopped = Trim(Left(sChopped, 18))
                sChopped += " UTC"
                Dim sYear As String = "20" + Mid(sChopped, 1, 2)
                Dim sMonth As String = Mid(sChopped, 4, 2)
                Dim sDay As String = Mid(sChopped, 7, 2)
                Dim sHour As String = Mid(sChopped, 10, 2)
                Dim sMinute As String = Mid(sChopped, 13, 2)
                Dim sSecond As String = Mid(sChopped, 16, 2)
                localDateTime = New Date(Val(sYear), Val(sMonth), Val(sDay), Val(sHour), Val(sMinute), Val(sSecond))
            End Using
        Return localDateTime
        Catch ex As Exception
            Return CDate("1-1-1970")
        End Try
    End Function
    Public Function VerifyPort(lPortNumber As Long, sHost As String) As Boolean

        Try
            Dim client = New TcpClient(sHost, lPortNumber)
            Using streamReader = New StreamReader(client.GetStream())
                Dim response = streamReader.ReadToEnd()
                'Note if we made it this far without an error, the result is positive
                Return True
            End Using
            Return True
        Catch ex As Exception
            Return False
        End Try
    End Function

End Class
