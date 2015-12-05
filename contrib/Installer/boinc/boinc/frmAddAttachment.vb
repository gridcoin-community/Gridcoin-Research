Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmAddAttachment

    Public Mode As String
    Public WithEvents cms As New ContextMenuStrip
    Private drHistory As DataTable
    Public sHandle As String = ""

    Public Function GetAttachmentName()
        Try

            Dim oAttach As SqlDataReader
            If Len(msPayload) = 0 Then Return "NA"
            oAttach = mGRCData.GetAttachment(msPayload)
            If oAttach.HasRows Then Return oAttach("BlobName") Else Return "N/A"

        Catch ex As Exception
            Return "N/A (Error)"
        End Try
    End Function

    Private Sub frmAddAttachment_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
        If msPayload <> "" Then
            '  If Mode = "View" Then
            txtAttachment.Text = GetAttachmentName()
            btnAddAttachment.Enabled = False
            lblMode.Text = "View Mode"
            'End If
        Else
            btnAddAttachment.Enabled = True
            lblMode.Text = "Add Mode"
        End If
    End Sub
    Private Sub frmAddAttachment_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        mGRCData = New GRCSec.GridcoinData
      End Sub
    Private Sub SetViewMode()
        Mode = "Add"
    End Sub
    Private Sub btnAddAttachment_Click(sender As System.Object, e As System.EventArgs) Handles btnAddAttachment.Click
        If Len(msPayload) = 0 Then msPayload = Guid.NewGuid.ToString()
        If mGRCData.IsAuthenticated(GetSessionGuid) = False Then
            If mfrmLogin Is Nothing Then
                mfrmLogin = New frmLogin
            End If
            mfrmLogin.Show()
            Me.Dispose()
            Exit Sub
        End If

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
                Dim sSuccess As String = mGRCData.mInsertAttachment(msPayload, OFD.SafeFileName, _
                                                             b, "", mGRCData.GetHandle(GetSessionGuid()))
                txtAttachment.Text = OFD.SafeFileName
                'Success

                Me.Enabled = False

                Dim sResult As String = ExecuteRPCCommand("addattachment", msPayload, "", "", "", "", "")

                Me.Hide()

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
                    If Len(msPayload) < 5 Then MsgBox("Attachment has been removed from the P2P server", MsgBoxStyle.Critical) : Exit Function
                    sFullPath = mRetrieveAttachment(msPayload, txtAttachment.Text, MerkleRoot)
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
            Dim bSuccess = mAttachmentSecurityScan(msPayload)
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
