Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmAddAnswer


    Public myGuid As String
    Public Mode As String
    Public WithEvents cms As New ContextMenuStrip
    Private drHistory As DataTable
    Public sHandle As String = ""
    Private sHistoryGuid As String

    Private Sub frmAddQuestion_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
        If mGRCData.IsAuthenticated(GetSessionGuid) = False Then
            Me.Hide()
            Me.Visible = False

            If mfrmLogin Is Nothing Then
                mfrmLogin = New frmLogin
            End If

            mfrmLogin.Show()

            Exit Sub
        End If
    End Sub

    Private Sub frmAddQuestion_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        mGRCData = New GRCSec.GridcoinData
        lblQuestion.Text = mGRCData.mGetQuestionText(myGuid)
        txtGRCAddress.Text = mGRCData.GetReceivingAddress(GetSessionGuid)


    End Sub

    Private Sub btnSubmit_Click(sender As System.Object, e As System.EventArgs) Handles btnSubmit.Click
        If Len(rtbNotes.Text) = 0 Then
            MsgBox("Answer must be provided.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If Len(txtGRCAddress.Text) < 10 Then
            MsgBox("GRC Address must be populated.", MsgBoxStyle.Exclamation)
            Exit Sub
        End If
        mGRCData.UpdateReceivingAddress(GetSessionGuid, txtGRCAddress.Text)


        If mGRCData.IsAuthenticated(GetSessionGuid) = False Then
            If mfrmLogin Is Nothing Then
                mfrmLogin = New frmLogin
            End If
            mfrmLogin.Show()
            Exit Sub
        End If
        If Len(myGuid) = 0 Then Exit Sub
        If Len(rtbNotes.Text) > 2999 Then
            MsgBox("Sorry, your answer must not exceed 3000 characters.  Please shorten it.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        mGRCData.mInsertAnswer(rtbNotes.Text, GetSessionGuid(), myGuid)

        btnSubmit.Enabled = True
        MsgBox("Success: Your Answer has been added.  ", MsgBoxStyle.Information)
        Me.Hide()
        mfrmFAQ.RefreshFAQ()

    End Sub


End Class
