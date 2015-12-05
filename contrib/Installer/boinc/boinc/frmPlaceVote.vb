Imports System.Windows.Forms.DataVisualization
Imports System.Windows.Forms.DataVisualization.Charting
Imports System.Drawing
Imports System.Windows.Forms

Public Class frmPlaceVote

    

    Public _id As String
    Private cbAnswers(20) As CheckBox

    Public Function ProperCase(sCase As String) As String
        If Len(sCase) > 1 Then sCase = UCase(Mid(sCase, 1, 1)) + Mid(sCase, 2, Len(sCase))
        Return sCase
    End Function
    Public Function PlaceVote(sTitle As String) As String

        Dim sData As String = GetPollData(sTitle)
        sData = Replace(sData, "_", " ")
        Dim sExpiration As String = ExtractXML(sData, "<EXPIRATION>")
        Dim sShareType As String = ExtractXML(sData, "<SHARETYPE>")
        Dim sQuestion As String = ExtractXML(sData, "<QUESTION>")
        Dim sAnswers As String = ExtractXML(sData, "<ANSWERS>")
        Dim sURL As String = ExtractXML(sData, "<URL>")

        'Array of answers
        Dim sArrayOfAnswers As String = ExtractXML(sData, "<ARRAYANSWERS>")
        Dim vAnswers() As String = Split(sArrayOfAnswers, "<RESERVED>")
        Dim sTotalParticipants As String = ExtractXML(sData, "<TOTALPARTICIPANTS>")
        Dim sTotalShares As String = ExtractXML(sData, "<TOTALSHARES>")
        Dim sBestAnswer As String = ExtractXML(sData, "<BESTANSWER>")
        'Display each answer
        Dim yOffset As Long = lblQuestion.Top + 25

        Dim iRow As Long
        For x As Integer = 0 To vAnswers.Length - 1
            Dim sAnswer As String = ExtractXML(vAnswers(x), "<ANSWERNAME>")

            If Len(sAnswer) > 0 Then
                iRow = iRow + 1
                Dim cbAnswer As New CheckBox

                cbAnswer.BackColor = Color.Black
                cbAnswer.ForeColor = Color.LightGreen
                cbAnswer.Font = New Font("Arial", 14)
                cbAnswer.Text = Trim(iRow) + ". " + sAnswer
                cbAnswer.Width = Me.Width * 0.75
                cbAnswer.Top = iRow * 34 + yOffset
                cbAnswer.Left = lblQuestion.Left + 50
                cbAnswer.Name = "cb" + Trim(iRow)
                cbAnswer.Tag = sAnswer
                AddHandler cbAnswer.Click, AddressOf ClickCheckbox
                cbAnswers(iRow) = cbAnswer
                Me.Controls.Add(cbAnswer)

            End If
        Next

        lblQuestion.Text = "Q: " + sQuestion
        lblTitle.Text = ProperCase(sTitle)
        lnkURL.Text = sURL

        Return ""
    End Function
    Private Sub ClickCheckbox(ByVal sender As Object, ByVal e As System.EventArgs)
        Dim cb As CheckBox = sender
        'Allow Multiple Choice
        For x As Integer = 1 To 20
            If Not cbAnswers(x) Is Nothing Then
                If cbAnswers(x).Name <> cb.Name Then
                    'cbAnswers(x).Checked = False
                End If
            End If
        Next
    End Sub
    Private Function GetVoteValue() As String
        Dim sAnswer As String = ""
        For x As Integer = 1 To 20
            If Not cbAnswers(x) Is Nothing Then
                If cbAnswers(x).Checked Then sAnswer += cbAnswers(x).Tag + ";"
            End If
        Next
        If Len(sAnswer) > 2 Then sAnswer = Mid(sAnswer, 1, Len(sAnswer) - 1)
        Return sAnswer
    End Function

    Function GetPollData(sTitle As String) As String
        If Not msGenericDictionary.ContainsKey("POLLS") Then
            MsgBox("No voting data exists.")
            Exit Function
        End If
        Dim sVoting As String = msGenericDictionary("POLLS")
        If Len(sVoting) = 0 Then Exit Function

        Dim vPolls() As String = Split(sVoting, "<POLL>")
        For y As Integer = 0 To vPolls.Length - 1
            Dim sTitle1 As String = ExtractXML(vPolls(y), "<TITLE>", "</TITLE>")
            Dim sURL As String = ExtractXML(vPolls(y), "<URL>", "</URL>")

            sTitle1 = Replace(sTitle1, "_", " ")
            sURL = Replace(sURL, "_", " ")

            If LCase(sTitle1) = LCase(sTitle) Then
                Return vPolls(y)
            End If
        Next y
        Return ""

    End Function

    Private Sub frmChartVotes_KeyDown(sender As Object, e As System.Windows.Forms.KeyEventArgs) Handles Me.KeyDown
        If e.KeyCode = Keys.Escape Then
            Me.Hide()
        End If
    End Sub

    Private Sub btnVote_Click(sender As System.Object, e As System.EventArgs) Handles btnVote.Click
        Dim sVote As String = GetVoteValue()
        If sVote = "" Then
            MsgBox("You must select an answer first.", MsgBoxStyle.Critical, "Gridcoin Voting System")
            Exit Sub
        End If
        Dim sTitle As String = lblTitle.Text
        sVote = Replace(sVote, " ", "_")
        sTitle = Replace(sTitle, " ", "_")
        Dim sResult As String = ExecuteRPCCommand("vote", sTitle, sVote, "", "", "", "")
        MsgBox(sResult, MsgBoxStyle.Information, "Gridcoin Voting System")

    End Sub

    Private Sub Button1_Click(sender As System.Object, e As System.EventArgs) Handles Button1.Click
       
        Dim sTitle As String = lblTitle.Text
        Dim sVote As String = "TEST"

        sTitle = Replace(sTitle, " ", "_")
        Dim sResult As String = ExecuteRPCCommand("vote", sTitle, sVote, "", "", "", "")
        MsgBox(sResult, MsgBoxStyle.Information, "Gridcoin Voting System")

    End Sub

    Private Sub lnkURL_LinkClicked(sender As System.Object, e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) Handles lnkURL.LinkClicked
        Process.Start(lnkURL.Text)
    End Sub
End Class