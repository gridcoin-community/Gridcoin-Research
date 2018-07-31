Module modBoincLeaderboard
    Public mlSqlBestBlock As Long = 0
    Public nBestBlock As Long = 0
    Public mlLeaderboardPosition As Long = 0
    Public mdScryptSleep As Double = 0
    Public msBlockSuffix As String = ""
    Public msSleepStatus As String = ""
    Public mdBlockSleepLevel As Double = 0
    Public msDefaultGRCAddress As String = ""
    Public msCPID As String = ""
    Public mlMagnitude As Double = 0

    Public Structure BoincProject
        Public URL As String
        Public Name As String
        Public Credits As Double
    End Structure

    Public Function CleanBody(sData As String) As String

        sData = Replace(sData, "'", "[Q]")
        sData = Replace(sData, "`", "[Q]")
        sData = Replace(sData, "<", "[LESSTHAN]")
        sData = Replace(sData, ">", "[GREATERTHAN]")
        sData = Replace(sData, "‘", "[Q]")
        sData = Replace(sData, "’", "[Q]")
        sData = Replace(sData, Chr(147), "[DQ]")
        sData = Replace(sData, Chr(148), "[DQ]")
        sData = Replace(sData, Chr(34), "[DQ]")
        Return sData

    End Function

    Public msTXID As String = ""

    Public bSqlHouseCleaningComplete As Boolean = False
    Public vProj() As String
    Public Function SQLInSync() As Boolean
        If mlSqlBestBlock < 800 Then Return False
        If nBestBlock < 800 Then Return False
        If mlSqlBestBlock > nBestBlock - 6 Then Return True
        Return False
    End Function
    Sub New()
        ReDim vProj(100)
        vProj(0) = "http://boinc.bakerlab.org/rosetta/   |rosetta@home"


    End Sub
    Public Function CodeToProject(sCode As String) As BoincProject
        Dim bp As New BoincProject

        Dim vRow() As String
        sCode = Trim(LCase(sCode))
        If sCode = "" Then Return bp

        For y As Integer = 0 To UBound(vProj)
            If Len(vProj(y)) > 10 Then
                vRow = Split(vProj(y), "|")
                If UBound(vRow) = 1 Then

                    If Left(LCase(vRow(1)), Len(sCode)) = sCode Then
                        bp.Name = Trim(vRow(1))
                        bp.URL = Trim(vRow(0))
                        Return bp
                    End If

                End If
            End If
        Next
        Return bp
    End Function
    Public Function GlobalizedDecimal(ByVal data As Object) As String
        Try
            Dim sOut As String
            sOut = Trim(data)
            If sOut.Contains(",") Then
                sOut = Replace(sOut, ",", "|")
                sOut = Replace(sOut, ".", "")
                sOut = Replace(sOut, "|", ".")

            End If

            Return sOut
        Catch ex As Exception
            Return Trim(data)
        End Try
    End Function
   

    Public Function UserAgent()
        'Reserved for iPhone and iPad use
        Return ""
    End Function
    Public Function Outdated(ByVal data As String, ByVal mins As Long) As Boolean
        Try

            If Trim(data) = "" Then Return True
            If IsDate(data) = False Then Return True
            Dim lMins As Long
            lMins = Math.Abs(DateDiff(DateInterval.Minute, Now, CDate(data)))

            If lMins > mins Then Return True
            Return False
        Catch ex As Exception
            Return True
        End Try

    End Function
    Public Function DatabaseExists(ByVal sDatabaseName As String) As Boolean
        Return System.IO.File.Exists(GetGridFolder() + "Sql\" + sDatabaseName)

    End Function
    'Copy the prod database to the read only database:
    Public Function ReplicateDatabase(ByVal sTargetDatabaseName As String)
        Dim sPath As String = GetGridFolder() + "Sql\gridcoinresearch"
        Dim sROPath As String = GetGridFolder() + "Sql\" + sTargetDatabaseName
        Try
            FileCopy(sPath, sROPath)
        Catch ex As Exception
        End Try
    End Function
    Public Function xUnlockDatabase()
        Dim sPath As String = GetGridFolder() + "Sql\gridcoinresearch"
        Dim sROPath As String = GetGridFolder() + "Sql\gridcoin_copy"
        Try
            If System.IO.File.Exists(sPath) = False Then Exit Function
            FileCopy(sPath, sROPath)
            System.IO.File.Delete(sPath)
            FileCopy(sROPath, sPath)
        Catch ex As Exception
            Log("UnlockDatabase:" + ex.Message)
        End Try
    End Function

End Module
