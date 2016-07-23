Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmAddExpense
    Public myGuid As String
    Public Mode As String
    Public WithEvents cms As New ContextMenuStrip
    Private drHistory As DataTable
    Public sHandle As String = ""
    Private sHistoryGuid As String

    Private Sub frmTicketAdd_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        Try
            mGRCData = New GRCSec.GridcoinData
        Catch ex As Exception
            MsgBox("Unable to load page " + ex.Message, MsgBoxStyle.Critical)
            Exit Sub
        End Try
        sHandle = KeyValue("TicketHandle")
        Try
            If Mode <> "View" Then
                If sHandle = "" Then
                    MsgBox("Click <OK> to populate your GridCoin Username.", MsgBoxStyle.Critical)
                    Dim frmU As New frmUserName
                    frmU.Show()
                    Me.Dispose()
                    Exit Sub
                End If
            End If

            txtSubmittedBy.Text = sHandle
            If myGuid <> "" Then
                txtTicketId.Text = myGuid
                If Mode = "View" Then
                    'Depersist from GFS
                    Dim oExpense As SqlDataReader = mGRCData.GetBusinessObject("Expense", txtTicketId.Text, "ExpenseID")
                    If oExpense.HasRows = False Then
                        MsgBox("Sorry, this expense does not exist.", MsgBoxStyle.Critical)
                        Exit Sub
                    End If
                    txtDescription.Text = oExpense("Descript")
                    txtAmount.Text = Trim(oExpense("Amount"))
                    txtAttachment.Text = Trim("" & oExpense("AttachmentName"))
                    txtSubmittedBy.Text = Trim("" & oExpense("SubmittedBy"))
                    rtbNotes.Text = Trim("" & oExpense("Comments"))
                    Dim sType As String = Trim("" & oExpense("Type"))
                    If sType = "Expense" Then rbExpense.Checked = True Else rbCampaign.Checked = True
                    btnSubmit.ForeColor = Drawing.Color.White
                    GroupBox1.Enabled = False
                    btnAddAttachment.Enabled = False
                    btnSubmit.Enabled = False
                    lblMode.Text = Mode + " Mode"
                End If
            Else
                txtTicketId.Text = Guid.NewGuid.ToString()
                btnAddAttachment.Enabled = True
                btnSubmit.ForeColor = Drawing.Color.Lime
                GroupBox1.Enabled = True
                btnSubmit.Enabled = True
                lblMode.Text = Mode + " Mode"
            End If

        Catch ex As Exception
            Me.Dispose()
        End Try
    End Sub

    Public Function NormalizeNote(sData As String)
        Dim sOut As String
        sOut = Replace(sData, "<LF>", vbCrLf)
        Return sOut
    End Function
    Public Function NormalizeNoteRow(sData As String)
        Dim sOut As String
        sOut = Replace(sData, "<LF>", "; ")
        Return sOut
    End Function

    Public Sub ShowTicket(sId As String)
        txtTicketId.Text = sId
        Call frmTicketAdd_Load(Me, Nothing)
        PopulateHistory()
        Me.Show()
        Dim dr As DataTable = mGetTicket(txtTicketId.Text)
        If dr Is Nothing Then Exit Sub
        txtSubmittedBy.Text = dr.Rows(0)("SubmittedBy")
        txtTicketId.Text = sId
        txtDescription.Text = dr.Rows(0)("Descript")
        Call PopulateHistory()
        SetViewMode()
        Me.TopMost = True
        Me.Refresh()
        Me.TopMost = False

    End Sub
    Private Sub SetViewMode()
        Mode = "Add"
        txtDescription.ReadOnly = True
    End Sub
    Public Sub AddTicket()
        Mode = "Add"
        If Mode = "Add" Then
            If Val(txtTicketId.Text) = 0 Then
                Dim maxTick As Double = mGRCData.P2PMax("TicketId", "Ticket", "", "")
                txtTicketId.Text = Trim(maxTick + 1)
            End If
        End If

    End Sub
    Private Sub btnSubmit_Click(sender As System.Object, e As System.EventArgs) Handles btnSubmit.Click
        ' Add the new campaign or expense
        If Len(txtSubmittedBy.Text) = 0 Then
            MsgBox("Submitted By must be populated.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If Len(txtAttachment.Text) = 0 Then
            MsgBox("Attachment must be provided.", MsgBoxStyle.Critical)
            Exit Sub
        End If

        If Len(txtDescription.Text) = 0 Or Len(rtbNotes.Text) = 0 Then
            MsgBox("Description And Notes must be populated with data.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If IsDate(dtStart.Text) = False Or IsDate(dtEnd.Text) = False Then
            MsgBox("Start and End Date must be populated", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If IsNumeric(txtAmount.Text) = False Then
            MsgBox("Amount must be populated", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If Val(txtAmount.Text) = 0 Then
            MsgBox("Amount must be greater than zero", MsgBoxStyle.Critical)
            Exit Sub
        End If

        If Not rbExpense.Checked And Not rbCampaign.Checked Then
            MsgBox("Please choose a type of submission: campaign or expense", MsgBoxStyle.Critical)
            Exit Sub
        End If
        Dim sType As String = IIf(rbExpense.Checked, "Expense", "Campaign")
        'Add the poll first, ensure it does not fail...
        'execute addpoll Expense_Authorization_[Foundation_blah] 21 Campaign_1 Approve;Deny 3
        Dim sTitle As String = IIf(sType = "Expense", "Expense_Submission" + txtDescription.Text, "New_Campaign_" + txtDescription.Text)
        sTitle = Replace(sTitle, " ", "_")
        sTitle += "[Foundation_" + txtTicketId.Text + "]"

        Dim sQuestion As String = IIf(sType = "Expense", "Approve_Expense?", "Approve_Campaign?")
        'if they are not logged on... throw an error ... 4-22-2016
        If sHandle = "" Then
            MsgBox("Please create a user name first.  After logging in you may re-submit the form.", MsgBoxStyle.Critical, "Not Logged In")
            Dim frmU1 As New frmUserName
            frmU1.Show()
            Exit Sub
        End If

        Dim sAnswers As String = "Approve;Deny"
        
        Try
            mGRCData.mInsertExpense(Mode, txtSubmittedBy.Text, txtTicketId.Text, "All", _
                        sType, txtDescription.Text, sType, rtbNotes.Text, MerkleRoot, _
                        Trim(dtStart.Text), Trim(dtEnd.Text), Trim(txtAmount.Text), rtbNotes.Text, txtAttachment.Text)

        Catch ex As Exception
            MsgBox("An error occurred while inserting the new expense [" + ex.Message + "]", MsgBoxStyle.Critical, "Gridcoin Foundation - Expense System")
            Exit Sub
        End Try


        Dim sResult As String = ExecuteRPCCommand("addpoll", sTitle, "21", sQuestion, sAnswers, "3", "http://www.gridcoin.us")
        If Not LCase(sResult).Contains("success") Then
            MsgBox(sResult, MsgBoxStyle.Information, "Gridcoin Foundation - Expense System")
            Exit Sub
        End If


        SetViewMode()

        btnSubmit.Enabled = True
        System.Windows.Forms.Cursor.Current = Cursors.Default
        MsgBox("Success: Your " + sType + " has been added.  ", MsgBoxStyle.Information)
        Me.Hide()

    End Sub

    Public Sub PopulateHistory()

        drHistory = mGetTicketHistory(txtTicketId.Text)
        If drHistory Is Nothing Then Exit Sub
        Dim sAttach As String = ""
        
        For i As Integer = 0 To drHistory.Rows.Count - 1
            Dim sRow As String = drHistory.Rows(i)("Disposition") + " - " _
                                 + Mid(NormalizeNoteRow(drHistory.Rows(i)("notes")), 1, 80) _
                                + " - " + drHistory.Rows(i)("AssignedTo") + " - " + drHistory.Rows(i)("updated")
            sAttach = drHistory.Rows(i)("BlobName")
            txtAttachment.Text = sAttach
            If Len(sAttach) > 1 Then sAttach = " - [" + sAttach + "]"
            sRow += sAttach

            Dim node As TreeNode = New TreeNode(sRow)
            node.Tag = i
            rtbNotes.Text = NormalizeNote(drHistory.Rows(i)("Notes"))
            'Verify security hash:
            Dim lAuthentic As Long = 0
            lAuthentic = mGRCSecurity.IsHashAuthentic("" & drHistory.Rows(i)("SecurityGuid"), _
                                                     "" & drHistory.Rows(i)("SecurityHash"), "" _
                                                     & drHistory.Rows(i)("PasswordHash"))
            'If lAuthentic <> 0 Then node.ForeColor = Drawing.Color.Red
        Next i

    End Sub
    Private Sub btnAddAttachment_Click(sender As System.Object, e As System.EventArgs) Handles btnAddAttachment.Click
        
        Dim sBlobGuid As String

        sBlobGuid = txtTicketId.Text
        If Len(sBlobGuid) < 5 Then MsgBox("You must select a historical ticket history item before attaching.", MsgBoxStyle.Critical) : Exit Sub

        Dim OFD As New OpenFileDialog()
        OFD.InitialDirectory = GetGridFolder()
        OFD.Filter = "CSV Files (*.csv)|*.csv|Adobe PDF Files (*.pdf)|*.pdf|Excel Files (*.xls)|*.xls|Excel Files (*.xlsx)|*.xlsx"
        OFD.FilterIndex = 2
        OFD.RestoreDirectory = True
        Me.TopMost = False

        If OFD.ShowDialog() = System.Windows.Forms.DialogResult.OK Then
            Try
                Dim b As Byte()
                b = FileToBytes(OFD.FileName)
                'Encrypt
                b = AES512EncryptData(b, MerkleRoot)
            
                Dim sSuccess As String = mGRCData.mInsertAttachment(sBlobGuid, OFD.SafeFileName, _
                                                             b, "", mGRCData.GetHandle(GetSessionGuid()))
                txtAttachment.Text = OFD.SafeFileName

            Catch Ex As Exception
                MessageBox.Show("Cannot read file from disk. Original error: " & Ex.Message)
            Finally
            End Try
        End If
    End Sub
    Private Function DownloadAttachment() As String
        Try

        If Len(txtAttachment.Text) > 1 Then
            'First does it exist already?
            Dim sDir As String = ""
            sDir = GetGridFolder() + "Attachments\"
            Dim sFullPath As String
            sFullPath = sDir + txtAttachment.Text
            If System.IO.Directory.Exists(sDir) = False Then MkDir(sDir)
            If Not System.IO.File.Exists(sFullPath) Then
                'Download from GFS
                Dim sID As String = txtTicketId.Text
                If Len(sID) < 5 Then MsgBox("Attachment has been removed from the P2P server", MsgBoxStyle.Critical) : Exit Function
                    sFullPath = mRetrieveAttachment(sID, txtAttachment.Text, MerkleRoot)
            End If
            Return sFullPath
        End If
        Catch ex As Exception
            MsgBox("Failed to download from GFS: " + ex.Message)
        End Try

    End Function
    Private Function AttachmentSecurityScan(bSecurityEnabled As Boolean) As Boolean
        If Len(txtAttachment.Text) = 0 Then Exit Function
        Try
            Dim sBlobGuid As String = ""
            Dim sID As String = txtTicketId.Text
            Dim bSuccess = mAttachmentSecurityScan(sID)
            If Not bSuccess And bSecurityEnabled Then
                MsgBox("! WARNING !   Attachment came from an unverifiable source.  Virus scan the file and use extreme caution before opening it.", MsgBoxStyle.Critical, "Authenticity Scan Failed")
            End If
            Return bSuccess
        Catch ex As Exception
            MsgBox("Document has failed security scanning and has been removed from the P2P server", MsgBoxStyle.Critical)
        End Try
    End Function
    Private Sub btnOpen_Click(sender As System.Object, e As System.EventArgs) Handles btnOpen.Click
        Try
            Dim sFullpath As String = DownloadAttachment()
            Dim bAuthScan As Boolean = AttachmentSecurityScan(False)
            'If Not bAuthScan Then Exit Sub 'dont let the user open it
            If System.IO.File.Exists(sFullpath) Then
                'Launch
                Process.Start(sFullpath)
            End If
        Catch ex As Exception
            MsgBox("Document has been removed from the P2P server " + ex.Message, MsgBoxStyle.Critical)

        End Try

    End Sub

    Private Sub btnOpenFolder_Click(sender As System.Object, e As System.EventArgs)
        Call AttachmentSecurityScan(False)

        Dim sFullpath As String = DownloadAttachment()
        If System.IO.File.Exists(sFullpath) Then
            Process.Start(GetGridFolder() + "Attachments\")
        Else
            MsgBox("File has been removed from the P2P Server", MsgBoxStyle.Critical)
        End If
    End Sub

    Private Sub btnVirusScan_Click(sender As System.Object, e As System.EventArgs) Handles btnVirusScan.Click
        '8-3-2015
        Dim sFullpath As String = DownloadAttachment()
        Dim sMd5 As String = GetMd5OfFile(sFullpath)
        Dim webAddress As String = "https://www.virustotal.com/latest-scan/" + sMd5
        Process.Start(webAddress)

    End Sub
End Class
