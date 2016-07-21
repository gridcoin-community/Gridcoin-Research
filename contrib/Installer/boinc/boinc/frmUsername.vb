Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmUserName

    Private Sub frmUserName_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        txtUserName.Text = KeyValue("TicketHandle")
    End Sub

    Private Sub btnAddAttachment_Click(sender As System.Object, e As System.EventArgs) Handles btnAddAttachment.Click
        If txtUserName.Text.Length < 3 Then
            MsgBox("UserName must be populated.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        UpdateKey("TicketHandle", txtUserName.Text)
        MsgBox("Username Updated!", MsgBoxStyle.Information)

    End Sub
End Class
