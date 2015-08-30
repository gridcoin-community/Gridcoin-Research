Module modBoincLeaderboard
    Public mlSqlBestBlock As Long = 0
    Public nBestBlock As Long = 0
    Public mlLeaderboardPosition As Long = 0
    Public mdScryptSleep As Double = 0
    Public msBlockSuffix As String = ""
    Public msSleepStatus As String = ""
    Public mdBlockSleepLevel As Double = 0
    Public mData As Sql
    Public msDefaultGRCAddress As String = ""
    Public msCPID As String = ""
    Public mlMagnitude As Double = 0
    Public mGRCData As GRCSec.GridcoinData
    Public mGRCSecurity As New GRCSec.GRCSec

    Public Structure BoincProject
        Public URL As String
        Public Name As String
        Public Credits As Double
    End Structure
    Public Function mInsertConfirm(dAmt As Double, sFrom As String, sTo As String, sTXID As String) As String
        If mData Is Nothing Then mData = New Sql
        Dim sInsert As String
        sInsert = "<INSERT><TABLE>Confirm</TABLE><FIELDS>GRCFrom,GRCTo,txid,amount,Confirmed</FIELDS><VALUES>'" + Trim(sFrom) + "','" + Trim(sTo) + "','" + Trim(sTXID) + "','" + Trim(dAmt) + "','0'</VALUES></INSERT>"
        Dim sErr As String
        sErr = mGRCData.Insert(sInsert)
        Return sErr
    End Function

    Public Function xmInsertUser(sHandle As String, sGRCaddress As String, CPID As String, sPassword As String) As String
        If mData Is Nothing Then mData = New Sql
        Dim sErr As String = ""
        Dim sInsert As String = ""
        'Store password hash only
        Dim sPassHash As String = ""
        sPassHash = GetMd5String(sPassword)
        Dim sUpdate As String
        sUpdate = "<UPDATE><TABLE>Users</TABLE><FIELDS>CPID,GRCAddress,Magnitude</FIELDS><VALUES>'" _
            + msCPID + "','" + msDefaultGRCAddress + "','" + Trim(mlMagnitude) + "'</VALUES><WHEREFIELDS>PasswordHash</WHEREFIELDS><WHEREVALUES>'" + sPassHash + "'</WHEREVALUES></UPDATE>"
        Return sErr
    End Function
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
    Public Sub mUpdateViaReaderx()
        Dim dr As GridcoinReader
        Dim sql As String
    End Sub
    Public Function mInsertTicket(sMode As String, sSubmittedBy As String, sTicketId As String, sAssignedTo As String, sDisposition As String, _
                                  sDesc As String, sType As String, sNotes As String, sPassword As String) As String
        If mData Is Nothing Then mData = New Sql
        Dim sErr As String = ""
        Dim sInsert As String
        sNotes = CleanBody(sNotes)

        If sMode = "Add" Then
            sInsert = "<INSERT><TABLE>Ticket</TABLE><FIELDS>TicketId,SubmittedBy,AssignedTo,Disposition,Descript,Type</FIELDS><VALUES>'" _
            + Trim(sTicketId) + "','" + Trim(sSubmittedBy) + "','" + Trim(sAssignedTo) + "','" + Trim(sDisposition) + "','" + Trim(sDesc) + "','" + Trim(sType) + "'</VALUES></INSERT>"
            sErr = mGRCData.Insert(sInsert)
            If Len(sErr) > 1 Then
                MsgBox(sErr, MsgBoxStyle.Critical, "Error while Creating Ticket")
                Exit Function
            End If
        Else
            sErr = mGRCData.UpdateTicket(sAssignedTo, sDisposition, sTicketId)
        End If

        'Retrieve ticket Guid
        Dim sGuid As String = mGRCData.P2PValue("id", "Ticket", "TicketId", sTicketId)
        If (Len(sGuid) > 10) Then
            'Calculate the Security Hash
            Dim sSecurityHash As String
            Dim grcSecurity As New GRCSec.GRCSec
            Dim sSecurityGuid As String = Guid.NewGuid.ToString()
            sSecurityHash = grcSecurity.CreateSecurityHash(sSecurityGuid, sPassword)

            sInsert = "<INSERT><TABLE>TicketHistory</TABLE><FIELDS>SubmittedBy,SecurityGuid,Parent,Disposition,AssignedTo,SecurityHash,Notes</FIELDS><VALUES>'" _
                + Trim(sSubmittedBy) + "','" + Trim(sSecurityGuid) + "','" _
                + Trim(sGuid) + "','" + Trim(sDisposition) + "','" + Trim(sAssignedTo) + "','" + Trim(sSecurityHash) + "','" + Trim(sNotes) + "'</VALUES></INSERT>"
            sErr += mGRCData.Insert(sInsert)

        End If
        Return sErr
    End Function
    Public Function GetSecurityToken(lSource As Long) As String
        Dim grcSecurity As New GRCSec.GRCSec
        Dim sSecurityGuid As String = Guid.NewGuid.ToString()
        Dim sSecurityHash As String
        sSecurityHash = grcSecurity.CreateSecurityHash(sSecurityGuid, GetSessionGuid())
        Dim sPassHash As String = GetMd5String(GetSessionGuid())
        Dim sToken As String
        sToken = "<SESSION><SOURCE>" + Trim(lSource) + "</SOURCE><SECURITYHASH>" + sSecurityHash + "</SECURITYHASH><SECURITYGUID>" + Trim(sSecurityGuid) + "</SECURITYGUID><PASSWORDHASH>" + Trim(sPassHash) + "</PASSWORDHASH></SESSION>"
        Return sToken
    End Function

    Public Function mRetrieveAttachment(sId As String, sName As String, sPass As String) As String

        Dim sPath As String = GetGridFolder() + "\Attachments"
        If System.IO.Directory.Exists(sPath) = False Then MkDir(sPath)
        Dim sFullPath As String = sPath + "\" + sName
        Dim sData As String = mGRCData.BoincBlob(sId)
        If InStr(1, sData, "<ERROR>") > 0 Then MsgBox(sData, MsgBoxStyle.Critical) : Exit Function
        Dim sBase64 As String = ExtractXML(sData, "<BLOB>", "</BLOB>")
        DecryptAES512AttachmentToFile(sFullPath, sBase64, sPass)
        Return sFullPath

    End Function

    Public msTXID As String = ""

    Public Sub mUpdateConfirmAsync()
        Dim sTxId As String = msTXID
        Threading.Thread.Sleep(9000) 'Wait 9 seconds asynchronously in case user sent coins to themself
        Try
            mGRCData.UpdateConfirm(sTxId, 1)
        Catch ex As Exception

        End Try
    End Sub
    Public Function mTrackConfirm(sTXID As String) As Double

        If mData Is Nothing Then mData = New Sql
        Dim dr As DataTable
        Dim sql As String
        sql = "Select Confirmed from Confirm where TXID='" + sTXID + "'"
        Try
            dr = mGRCData.GetDataTable(sql)
        Catch ex As Exception
            Return -1
        End Try
        Try
            Return Val(dr.Rows(0)(0).ToString())
        Catch ex As Exception
            Log("HEINOUS 2:" + ex.Message)
            Return 0
        End Try
    End Function

    Public Function mGetFilteredTickets(sFilter As String, sAssignedTo As String) As DataTable
        If mData Is Nothing Then mData = New Sql
        Dim dr As DataTable = Nothing
        Dim sql As String
        Dim sClause As String = ""
        If Len(sAssignedTo) > 1 Then
            sClause = " and id in (select a.parent from ( select Parent,max(updated) as maxdate      FROM ticketHistory" _
                & "   group by ticketHistory.parent) " _
                & "   a     inner join TicketHistory as TH on th.parent = A.parent and th.updated = a.maxdate   where th.assignedTo='" + sAssignedTo + "'   ) "
        End If
        sql = "Select * From Ticket " + sFilter + " " + sClause + " AND Disposition <> 'Closed' ORDER BY UPDATED DESC "
        Try
            Log(sql)

            dr = mGRCData.GetDataTable(sql)
        Catch ex As Exception
            Log(sql + " : err Description: " + ex.Message)
            Return dr
        End Try
        Return dr
    End Function
    Public Function mGetTicketHistory(sTicketID As String) As DataTable
        Dim myGuid As String
        myGuid = mGRCData.P2PValue("id", "Ticket", "TicketId", sTicketID)
        Dim dr As New DataTable
        If myGuid = "" Then Return dr
        Dim sql As String
        sql = "Select Users.PasswordHash,TicketHistory.*,Attachment.Id as BlobGuid,Attachment.BlobName From TicketHistory left join attachment on Attachment.Parent = TicketHistory.id   left join users on users.handle = tickethistory.Submittedby  where TicketHistory.Parent = '" + myGuid + "'  Order by Added"
        Try
            dr = mGRCData.GetDataTable(sql)
        Catch ex As Exception
            Return dr
        End Try
        Return dr
    End Function
    Public Function mAttachmentSecurityScan(sBlobId As String) As Boolean
        Dim gr As SqlClient.SqlDataReader
        gr = mGRCData.GetAttachment(sBlobId)
        Dim lAuthentic As Long
        lAuthentic = mGRCSecurity.IsHashAuthentic("" & gr("SecurityGuid"), _
                                                   "" & gr("SecurityHash"), gr("PasswordHash"))

        If lAuthentic <> 0 Then Return False Else Return True

    End Function
   
    Public Function mGetTicket(sTicketID As String) As DataTable
        If mData Is Nothing Then mData = New Sql
        Dim dr As DataTable
        Dim sql As String
        sql = "Select * From Ticket where ticketid='" + sTicketID + "'"
        Try
            dr = mGRCData.GetDataTable(sql)
        Catch ex As Exception
            Return dr
        End Try
        Return dr
    End Function


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
