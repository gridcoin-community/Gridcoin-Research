Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmAddQuestion

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
        
    End Sub

    Private Sub btnSubmit_Click(sender As System.Object, e As System.EventArgs) Handles btnSubmit.Click
        If Len(rtbNotes.Text) = 0 Then
            MsgBox("Question must be provided.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If mGRCData.IsAuthenticated(GetSessionGuid) = False Then
            If mfrmLogin Is Nothing Then
                mfrmLogin = New frmLogin
            End If
            mfrmLogin.Show()
            Exit Sub
        End If

        If Len(rtbNotes.Text) > 1000 Then
            MsgBox("Sorry your question must not exceed 1000 characters.", MsgBoxStyle.Exclamation)
            Exit Sub
        End If

        mGRCData.mInsertQuestion(rtbNotes.Text, GetSessionGuid())

        btnSubmit.Enabled = True
        MsgBox("Success: Your Question has been added.  ", MsgBoxStyle.Information)
        Me.Hide()
        mfrmFAQ.RefreshFAQ()

    End Sub

    
End Class
