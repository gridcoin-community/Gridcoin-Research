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
        sErr = mData.ExecuteP2P(sInsert, Nothing, 2)
        Return sErr
    End Function

    Public Function mInsertUser(sHandle As String, sGRCaddress As String, CPID As String, sPassword As String) As String
        If mData Is Nothing Then mData = New Sql
        Dim sErr As String = ""
        Dim sInsert As String
        'Store password hash only
        Dim sPassHash As String = ""
        sPassHash = GetMd5String(sPassword)

        sInsert = "<INSERT><TABLE>Users</TABLE><FIELDS>Handle,GRCAddress,CPID,PasswordHash</FIELDS><VALUES>'" _
            + Trim(sHandle) + "','" + Trim(sGRCaddress) + "','" + CPID + "','" + sPassHash + "'</VALUES></INSERT>"
        sErr = mData.ExecuteP2P(sInsert, Nothing, 1)

        If Len(sErr) > 1 Then
            'continue...Violation of PK
        End If
        Dim sUpdate As String
        sUpdate = "<UPDATE><TABLE>Users</TABLE><FIELDS>PasswordHash,CPID,GRCAddress,Magnitude</FIELDS><VALUES>'" + sPassHash + "','" + msCPID + "','" + msDefaultGRCAddress + "','" + Trim(mlMagnitude) + "'</VALUES><WHEREFIELDS>PasswordHash</WHEREFIELDS><WHEREVALUES>'" + sPassHash + "'</WHEREVALUES></UPDATE>"
        sErr = mData.ExecuteP2P(sUpdate, Nothing, 1)

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
    Public Sub mUpdateViaReader()
        Dim dr As GridcoinReader
        Dim sql As String
        ' sql = "UPDATE USERS SET PasswordHash='" + Trim(sHash) + "' where Confirm where TXID='" + sTXID + "'"
       
        '     dr = mData.GetGridcoinReader(sql)
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
            sErr = mData.ExecuteP2P(sInsert, Nothing, 1)
            If Len(sErr) > 1 Then
                MsgBox(sErr, MsgBoxStyle.Critical, "Error while Creating Ticket")
                Exit Function
            End If
        Else
            Dim sUpdate As String = ""
            sUpdate = "<UPDATE><TABLE>Ticket</TABLE><FIELDS>AssignedTo,Disposition</FIELDS><VALUES>'" + sAssignedTo + "','" + Trim(sDisposition) + "'</VALUES><WHEREFIELDS>TicketId</WHEREFIELDS><WHEREVALUES>'" + sTicketId + "'</WHEREVALUES></UPDATE>"
            sErr = mData.ExecuteP2P(sUpdate, Nothing, 1)
        End If

        'Retrieve ticket Guid
        Dim sGuid As String = P2PValue("id", "Ticket", "TicketId", sTicketId)
        If (Len(sGuid) > 10) Then
            'Calculate the Security Hash
            Dim sSecurityHash As String
            Dim grcSecurity As New GRCSec.GRCSec
            Dim sSecurityGuid As String = Guid.NewGuid.ToString()
            sSecurityHash = grcSecurity.CreateSecurityHash(sSecurityGuid, sPassword)

            sInsert = "<INSERT><TABLE>TicketHistory</TABLE><FIELDS>SubmittedBy,SecurityGuid,Parent,Disposition,AssignedTo,SecurityHash,Notes</FIELDS><VALUES>'" _
                + Trim(sSubmittedBy) + "','" + Trim(sSecurityGuid) + "','" _
                + Trim(sGuid) + "','" + Trim(sDisposition) + "','" + Trim(sAssignedTo) + "','" + Trim(sSecurityHash) + "','" + Trim(sNotes) + "'</VALUES></INSERT>"
            sErr += mData.ExecuteP2P(sInsert, Nothing, 1)

        End If
        Return sErr
    End Function
    Public Function GetSecurityToken(lSource As Long) As String
        Dim grcSecurity As New GRCSec.GRCSec
        Dim sSecurityGuid As String = Guid.NewGuid.ToString()
        Dim sSecurityHash As String
        sSecurityHash = grcSecurity.CreateSecurityHash(sSecurityGuid, KeyValue("TicketPassword"))
        Dim sPassHash As String = GetMd5String(KeyValue("TicketPassword"))
        Dim sToken As String
        sToken = "<SESSION><SOURCE>" + Trim(lSource) + "</SOURCE><SECURITYHASH>" + sSecurityHash + "</SECURITYHASH><SECURITYGUID>" + Trim(sSecurityGuid) + "</SECURITYGUID><PASSWORDHASH>" + Trim(sPassHash) + "</PASSWORDHASH></SESSION>"
        Return sToken
    End Function

    Public Function mInsertAttachment(sParentId As String, sName As String, blob() As Byte, sPassword As String, sSubmittedBy As String) As String
        If mData Is Nothing Then mData = New Sql
        Dim sErr As String = ""
        Dim sInsert As String '<BLOB></BLOB>
        'Calculate the Security Hash
        Dim sSecurityHash As String
        Dim grcSecurity As New GRCSec.GRCSec
        Dim sSecurityGuid As String = Guid.NewGuid.ToString()
        sSecurityHash = grcSecurity.CreateSecurityHash(sSecurityGuid, sPassword)

        sInsert = "<INSERT><TABLE>Attachment</TABLE><FIELDS>SubmittedBy,SecurityGuid,SecurityHash,Parent,BlobName</FIELDS><VALUES>'" + Trim(sSubmittedBy) + "','" + Trim(sSecurityGuid) + "','" + Trim(sSecurityHash) + "','" + Trim(sParentId) + "','" + Trim(sName) _
            + "'</VALUES></INSERT>"
        sErr += mData.ExecuteP2P(sInsert, blob, 1)

        Return sErr
    End Function


    Public Function mRetrieveAttachment(sId As String, sName As String, sPass As String) As String

        Dim sPath As String = GetGridFolder() + "\Attachments"
        If System.IO.Directory.Exists(sPath) = False Then MkDir(sPath)
        Dim sFullPath As String = sPath + "\" + sName
        Dim sData As String = mData.BoincBlob(sId)
        If InStr(1, sData, "<ERROR>") > 0 Then MsgBox(sData, MsgBoxStyle.Critical) : Exit Function
        Dim sBase64 As String = ExtractXML(sData, "<BLOB>", "</BLOB>")
        DecryptAES512AttachmentToFile(sFullPath, sBase64, sPass)

        Return sFullPath

    End Function



    Public msTXID As String = ""

    Public Function mUpdateConfirmAsync()
        Dim sTxId As String = msTXID
        Threading.Thread.Sleep(9000) 'Wait 9 seconds asynchronously in case user sent coins to themself
        mUpdateConfirm(sTxId, 1)
    End Function
    Public Function mUpdateConfirm(sTXID As String, iStatus As Long) As String
        sTXID = Replace(sTXID, Chr(0), "")
        If mData Is Nothing Then mData = New Sql
        Dim sUpdate As String
        sUpdate = "<UPDATE><TABLE>Confirm</TABLE><FIELDS>Confirmed</FIELDS><VALUES>'1'</VALUES><WHEREFIELDS>TXID</WHEREFIELDS><WHEREVALUES>'" + Trim(sTXID) + "'</WHEREVALUES></UPDATE>"
        Dim sErr As String
        Log("Updating " + sUpdate)
        sErr = mData.ExecuteP2P(sUpdate, Nothing, 2)

        Log("Done " + Trim(sErr))
        Return sErr
    End Function
    Public Function mTrackConfirm(sTXID As String) As Double

        If mData Is Nothing Then mData = New Sql

        Dim dr As GridcoinReader
        Dim sql As String
        sql = "Select Confirmed from Confirm where TXID='" + sTXID + "'"
        Log("Tracking " + sql)

        Try
            dr = mData.GetGridcoinReader(sql, 10)

        Catch ex As Exception
            Return -1
        End Try

        Try
            Dim grr As New GridcoinReader.GridcoinRow
            grr = dr.GetRow(1)
            Log("Data returned " + Trim(grr.Values(0).ToString))

            Return Val(grr.Values(0).ToString())

        Catch ex As Exception
            Log("HEINOUS 2:" + ex.Message)
            Return 0
        End Try

    End Function

    Public Function mGetFilteredTickets(sFilter As String, sAssignedTo As String) As GridcoinReader
        If mData Is Nothing Then mData = New Sql

        Dim dr As GridcoinReader
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

            dr = mData.GetGridcoinReader(sql, 1)
        Catch ex As Exception
            Log(sql + " : err Description: " + ex.Message)

            Return dr
        End Try
        Return dr
    End Function
    Public Function mGetTicketHistory(sTicketID As String) As GridcoinReader
        If mData Is Nothing Then mData = New Sql
        Dim myGuid As String
        myGuid = P2PValue("id", "Ticket", "TicketId", sTicketID)

        Dim dr As GridcoinReader
        If myGuid = "" Then Return dr

        Dim sql As String
        sql = "Select Users.PasswordHash,TicketHistory.*,Attachment.Id as BlobGuid,Attachment.BlobName From TicketHistory left join attachment on Attachment.Parent = TicketHistory.id   left join users on users.handle = tickethistory.Submittedby  where TicketHistory.Parent = '" + myGuid + "'  Order by Added"


        Try
            dr = mData.GetGridcoinReader(sql, 1)
        Catch ex As Exception
            Return dr
        End Try
        Return dr
    End Function
    Public Function mGetAttachment(sBlobId As String) As GridcoinReader
        If mData Is Nothing Then mData = New Sql
        Dim dr As GridcoinReader
        Dim sql As String
        sql = "select attachment.*,users.passwordHash from attachment left join users on users.handle = attachment.Submittedby where attachment.Id='" + sBlobId + "'"
        Try
            dr = mData.GetGridcoinReader(sql, 1)
        Catch ex As Exception
            Return dr
        End Try
        Return dr
    End Function
    Public Function mAttachmentSecurityScan(sBlobId As String) As Boolean
        Dim gr As GridcoinReader
        gr = mGetAttachment(sBlobId)
        Dim grcSecurity As New GRCSec.GRCSec
        Dim lAuthentic As Long

        lAuthentic = grcSecurity.IsHashAuthentic("" & gr.Value(1, "SecurityGuid"), _
                                                             "" & gr.Value(1, "SecurityHash"), "" & gr.Value(1, "PasswordHash"))

        If lAuthentic <> 0 Then Return False Else Return True

    End Function
    Public Function mGetUsers() As GridcoinReader
        If mData Is Nothing Then mData = New Sql
        Dim dr As GridcoinReader
        Dim sql As String
        sql = "Select * From Users where deleted=0 and isnull(handle,'') <> '' and passwordhash is not null "
        Try
            dr = mData.GetGridcoinReader(sql, 1)
        Catch ex As Exception
            Return dr
        End Try
        Return dr
    End Function



    Public Function mGetTicket(sTicketID As String) As GridcoinReader
        If mData Is Nothing Then mData = New Sql
        Dim dr As GridcoinReader
        Dim sql As String
        sql = "Select * From Ticket where ticketid='" + sTicketID + "'"
        Try
            dr = mData.GetGridcoinReader(sql, 1)
        Catch ex As Exception
            Return dr
        End Try
        Return dr
    End Function

    Public Function P2PMax(LookupField As String, sTable As String, sSearchField As String, sTargetValue As String) As String
        If mData Is Nothing Then mData = New Sql

        Dim dr As GridcoinReader
        Dim sql As String
        sql = "Select Max(Cast (" + LookupField + " as money)) from " + sTable + " where deleted=0 "

        Try
            dr = mData.GetGridcoinReader(sql, 1)

        Catch ex As Exception
            Return -1
        End Try

        Try
            Dim grr As New GridcoinReader.GridcoinRow
            grr = dr.GetRow(1)

            Return grr.Values(0).ToString()

        Catch ex As Exception
            Log("HEINOUS 2:" + ex.Message)
            Return ""
        End Try

    End Function
    Public Function P2PValue(LookupField As String, sTable As String, sSearchField As String, sTargetValue As String) As String
        If mData Is Nothing Then mData = New Sql

        Dim dr As GridcoinReader
        Dim sql As String
        sql = "Select " + LookupField + " from " + sTable + " where deleted=0 and " + sSearchField + " ='" + sTargetValue + "'"
        Log("Tracking " + sql)

        Try
            dr = mData.GetGridcoinReader(sql, 1)

        Catch ex As Exception
            Return -1
        End Try

        Try
            Dim grr As New GridcoinReader.GridcoinRow
            grr = dr.GetRow(1)

            Return grr.Values(0).ToString()

        Catch ex As Exception
            Log("HEINOUS 2:" + ex.Message)
            Return ""
        End Try

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
    Public Function RefreshLeaderboardFactors(d As Sql)

        Dim sql As String = ""


        Try

            Dim vRow() As String
            Dim dAvg As Double
            '''''''''''''''''''''''''''1-22-2014 D37D: Purge duplicate Host Records before trusting data:
            sql = "Select id,host,address      from leaderboard    group by address     order by host,id"

            Dim gr1 As New GridcoinReader
            gr1 = d.GetGridcoinReader(sql, 10)
            Dim grr1 As GridcoinReader.GridcoinRow
            Dim grForwardRow As GridcoinReader.GridcoinRow
            Dim lPurged As Long

            For y1 As Integer = 1 To gr1.Rows - 1
                grr1 = gr1.GetRow(y1)
                grForwardRow = gr1.GetRow(y1 + 1)
                Dim sHost As String
                sHost = gr1.Value(y1, "Host")
                Dim sForwardHost As String
                sForwardHost = gr1.Value(y1 + 1, "Host")
                If sForwardHost = sHost And gr1.Value(y1, "Address") <> gr1.Value(y1 + 1, "Address") Then
                    'This host changed GRC address during the month! Purge older records:
                    sql = "Delete from Leaderboard where Address = '" + Trim(gr1.Value(y1, "Address")) + "'"
                    Log(sql)

                    Log("Purging " + gr1.Value(y1, "Address"))

                    lPurged = lPurged + 1
                    d.Exec(sql)
                End If

            Next y1
            Log("Purged(b) " + Trim(lPurged) + " records.")

            ''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

            sql = "Select  avg(credits) as [Credits] from  leaderboard  "
            dAvg = Val(d.QueryFirstRow(sql, "Credits"))

            For y As Integer = 0 To UBound(vProj)
                If Len(vProj(y)) > 10 Then
                    vRow = Split(vProj(y), "|")
                    If UBound(vRow) = 1 Then
                        Dim sProjName As String = vRow(1)
                        sql = "Update Leaderboard set Factor = (Select (" + GlobalizedDecimal(dAvg) _
                            + "/avg(credits)) as [Credits] from  leaderboard LL " _
                            & " where LL.projectname='" + Trim(sProjName) + "') WHERE ProjectName = '" + sProjName + "'"
                        d.Exec(sql)
                    End If
                End If
            Next y
            sql = "Update leaderboard set Factor = 4 where Factor > 4"
            d.Exec(sql)
            sql = "Update Leaderboard set AdjCredits = Factor*Credits where 1=1 "
            d.Exec(sql)
            'Update scrypt sleep for the network:
            sql = " Select avg(credits*factor*projectcount) as [AdjCredits], Address from " _
                & " leaderboard group by Address order by avg(credits*factor*projectcount) desc "
            Dim gr As New GridcoinReader
            gr = d.GetGridcoinReader(sql, 10)
            Dim grr As GridcoinReader.GridcoinRow
            Dim sleepfactor = gr.Rows / 50
            Dim chance As Double = 0
            Dim sleeppercent As Double = 0

            For y As Integer = 1 To gr.Rows
                grr = gr.GetRow(y)
                chance = y / sleepfactor
                sleeppercent = 100 - chance
                If sleeppercent > 94.5 Then sleeppercent = 100 'This allows the top 10% to hash 100% of the time
                If sleeppercent < 50 Then sleeppercent = 50
                'Globalizaton: store as a decimal from 0 - 1 (meaning 0-100%)
                sleeppercent = sleeppercent / 100
                '1-19-2013
                sql = "Update Leaderboard Set ScryptSleepChance = " + GlobalizedDecimal(sleeppercent) _
                    + " where address = '" + gr.Value(y, "Address") + "'"
                d.Exec(sql)
            Next y

            gr = Nothing
            grr = Nothing
        Catch ex As Exception
            Log("Refresh Leaderboard factors: " + ex.Message + " " + sql + ex.InnerException.ToString)
        End Try


    End Function
    Public Function RefreshLeaderboard()

        If KeyValue("UpdatingLeaderboard") = "true" Then Exit Function

        Try

            UpdateKey("UpdatingLeaderboard", "true")



            Log("Updating Leaderboard")
            Dim sql As String
            Dim d As New Sql
            d.CreateLeaderboardTable()

            Dim dBlock As Double = d.HighBlockNumber
            Dim lBlock As Double = dBlock - 8640 '15 days back
            If lBlock < 101 Then lBlock = 101

            sql = "Delete from Leaderboard" 'Truncate Table
            d.Exec(sql)
            '''''''''Fake temporary data useful for sample queries until all the clients sync blocks into sql server: (1-1-2014)
            If False Then
                Dim sHash2 As String
                sHash2 = "xa3,1, 98, CRD_V, SOLO_MIN, GBZkHyR7sKXfdh1Z7FMxbsLB,  23, 2854:2963:2969  ,483CB1696,310830774fefd00fb888761f0e\1:3a94913164b731f5c712e4a7852575a3\50\2969\3\58842\World_1000_175:MILKY_2000_275:SETI_3000_375\1386004003\2\270722"
                For x = 1 To 10
                    sql = "Update Blocks set boinchash = '" + sHash2 + "' where height = '" + GlobalizedDecimal(x) + "';"
                    d.Exec(sql)
                Next
                lBlock = 0
            Else
                If lBlock < 100 Then Exit Function
            End If
            '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
            sql = "Select height, Boinchash From blocks where height > " + GlobalizedDecimal(lBlock) + " order by height;"
            Dim dr As GridcoinReader
            Log("Gathering data for Leaderboard: " + Trim(sql))

            dr = d.GetGridcoinReader(sql, 11)

            Dim sqlI As String = ""
            Dim sHash As String
            Dim vHash() As String
            Dim sbi As New System.Text.StringBuilder
            Dim grr As GridcoinReader.GridcoinRow
            For y = 1 To dr.Rows
                grr = dr.GetRow(y)
                sHash = dr.Value(y, "boinchash")
                vHash = Split(sHash, ",")
                Dim sSourceBlock As String
                Dim vSourceBlock() As String
                If UBound(vHash) >= 9 Then
                    sSourceBlock = vHash(9)
                    If UBound(vHash) > 17 Then
                        If InStr(1, vHash(17), "\") > 0 Then
                            sSourceBlock = vHash(17) 'Support for Pools
                        End If
                    End If
                    vSourceBlock = Split(sSourceBlock, "\")
                    Dim sExpandedProjects As String
                    Dim vExpandedProjects() As String
                    If UBound(vSourceBlock) > 8 Then
                        sExpandedProjects = vSourceBlock(6)
                        'sExpandedProjects = "World_1000_100:MILKY_2000_200:SETI_3000_300"
                        If sExpandedProjects.Contains("_") Then
                            vExpandedProjects = Split(sExpandedProjects, ":")
                            Dim sProjData As String
                            Dim vProjData() As String
                            'Precalculate active project count:
                            Dim lProjCount As Long = 0

                            For pr As Integer = 0 To UBound(vExpandedProjects)
                                sProjData = vExpandedProjects(pr)
                                vProjData = Split(sProjData, "_")
                                If UBound(vProjData) = 2 Then
                                    Dim dCredits As Double
                                    Dim sProject As String
                                    Dim sHost As String
                                    sHost = vProjData(2)
                                    sProject = vProjData(0)
                                    dCredits = Val(vProjData(1))
                                    If dCredits > 0 Then lProjCount += 1
                                End If
                            Next pr
                            For x As Integer = 0 To UBound(vExpandedProjects)
                                sProjData = vExpandedProjects(x)
                                vProjData = Split(sProjData, "_")
                                If UBound(vProjData) = 2 Then
                                    Dim dCredits As Double
                                    Dim sProject As String
                                    Dim sHost As String
                                    sHost = vProjData(2)
                                    sProject = vProjData(0)
                                    dCredits = Val(vProjData(1))
                                    Dim sGRCAddress As String
                                    sGRCAddress = vHash(5)
                                    If UBound(vHash) > 17 Then
                                        If InStr(1, vHash(17), "\") > 0 Then
                                            If Len(vHash(13)) > 0 Then
                                                sGRCAddress = vHash(13) 'Pool Support
                                                'select * from leaderboard where address='FupazJkUW4bP3JJgHPkh4JvM8kA33ztHRj'

                                            End If

                                        End If
                                    End If

                                    Dim bp As New BoincProject
                                    bp = CodeToProject(sProject)
                                    If Len(bp.URL) > 1 Then
                                        If dCredits > 0 Then
                                            ' ProjectCount integer, Factor numeric(12,2), AdjCredits
                                            Dim sSql As String
                                            sSql = "Insert into LeaderBoard (Added, Address, Host, Project, Credits, ProjectName, ProjectURL, ProjectCount, Factor, AdjCredits) VALUES " _
                                                              & "(date('now'),'" + sGRCAddress + "','" + sHost + "','" + sProject + "','" _
                                                              + GlobalizedDecimal(dCredits) + "','" + bp.Name _
                                                              + "','" + bp.URL + "'," + GlobalizedDecimal(lProjCount) + ",'0','0');"
                                            sbi.AppendLine(sSql)

                                        End If

                                    End If
                                End If
                            Next
                        End If
                    End If
                End If
            Next y

            d.ExecHugeQuery(sbi)
            'Update Project Count
            Log("Updating factors")

            RefreshLeaderboardFactors(d)
            'd.UpdateUserSummary()
            Log("Updated Leaderboard")

            Try
                ReplicateDatabase("gridcoin_leaderboard")
            Catch ex As Exception

            End Try

            'Update the sync key

            UpdateKey("UpdatedLeaderboard", Trim(Now))
            d = Nothing
            UpdateKey("UpdatingLeaderboard", "false")

        Catch ex As Exception
            Log("Refresh leaderboard: " + ex.Message)
            UpdateKey("UpdatingLeaderboard", "false")

        End Try
        UpdateKey("UpdatingLeaderboard", "false")

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
