Imports System.Windows.Forms.DataVisualization
Imports System.Windows.Forms.DataVisualization.Charting
Imports System.Drawing
Imports System.Windows.Forms

Public Class frmChartVotes

    Public _id As String

    Public Function ProperCase(sCase As String) As String
        If Len(sCase) > 1 Then sCase = UCase(Mid(sCase, 1, 1)) + Mid(sCase, 2, Len(sCase))
        Return sCase
    End Function
    Public Function ChartPoll(sTitle As String) As String

        Dim sData As String = GetPollData(sTitle)
        sData = Replace(sData, "_", " ")
        Dim sExpiration As String = ExtractXML(sData, "<EXPIRATION>")
        Dim sShareType As String = ExtractXML(sData, "<SHARETYPE>")
        Dim sQuestion As String = ExtractXML(sData, "<QUESTION>")
        Dim sURL As String = ExtractXML(sData, "<URL>")

        Dim sAnswers As String = ExtractXML(sData, "<ANSWERS>")
        'Array of answers
        Dim sArrayOfAnswers As String = ExtractXML(sData, "<ARRAYANSWERS>")
        Dim vAnswers() As String = Split(sArrayOfAnswers, "<RESERVED>")
        Dim sTotalParticipants As String = ExtractXML(sData, "<TOTALPARTICIPANTS>")
        Dim sTotalShares As String = ExtractXML(sData, "<TOTALSHARES>")
        Dim sBestAnswer As String = ExtractXML(sData, "<BESTANSWER>")
        lblBestAnswer.Text = "Best Answer: " + sBestAnswer
        lblQuestion.Text = "Q: " + sQuestion
        lblTitle.Text = ProperCase(sTitle)
        lnkURL.Text = sURL

        Dim iRow As Long = 0
        C.Titles.Clear() : C.Titles.Add("Poll Results " + sTitle) : C.BackColor = Color.Black : C.ForeColor = Color.Lime
        C.ChartAreas(0).AxisX.IntervalType = DateTimeIntervalType.Auto
        C.ChartAreas(0).BackSecondaryColor = Color.Black
        C.ChartAreas(0).AxisX.LabelStyle.ForeColor = Color.Lime
        C.ChartAreas(0).AxisY.LabelStyle.ForeColor = Color.Black
        C.ChartAreas(0).AxisY.LabelStyle.ForeColor = Color.Lime : C.ChartAreas(0).ShadowColor = Color.Chocolate
        If C.Legends.Count = 1 Then
            C.Legends.Add("Poll Results " + sTitle) : C.Legends(0).ForeColor = Color.Lime
        End If

        C.ChartAreas(0).BackColor = Color.Black
        C.Legends(0).BackColor = Color.Black
        C.ChartAreas(0).AxisX.LabelStyle.IntervalType = DateTimeIntervalType.Auto
        C.ChartAreas(0).AxisY.LabelStyle.IsEndLabelVisible = False
        C.ChartAreas(0).Area3DStyle.Enable3D = True
        'Vertical inclination
        C.ChartAreas(0).Area3DStyle.Inclination = 7
        'horizontal rotation
        C.ChartAreas(0).Area3DStyle.Rotation = 87
        C.ChartAreas(0).AxisY.LabelStyle.IntervalType = DateTimeIntervalType.NotSet
        C.Series.Clear()
        'Create series for each answer
        Dim serPrice(vAnswers.Length - 1) As Series
        For subY As Integer = 0 To vAnswers.Length - 1
            If Len(vAnswers(subY)) > 0 Then
                iRow += 1
                Dim sAnswerName As String = ExtractXML(vAnswers(subY), "<ANSWERNAME>")
                Dim sParticipants As String = ExtractXML(vAnswers(subY), "<PARTICIPANTS>")
                Dim dShares As String = CDbl(ExtractXML(vAnswers(subY), "<SHARES>"))
                If dShares = 0 Then dShares = 1
                If True Or serPrice(iRow) Is Nothing Then
                    serPrice(iRow) = New Series
                    serPrice(iRow).Name = sAnswerName
                    serPrice(iRow).ChartType = SeriesChartType.Column

                    Dim c1 As Integer = Rnd(1) * 255
                    Dim c2 As Integer = Rnd(1) * 255
                    Dim c3 As Integer = Rnd(1) * 255
                    serPrice(iRow).Color = ColorTranslator.FromOle(RGB(c1, c2, c3))
                    serPrice(iRow).LabelForeColor = ColorTranslator.FromOle(RGB(c3, c2, c1))
                    serPrice(iRow).LegendText = sAnswerName
                    serPrice(iRow).IsVisibleInLegend = True
                    serPrice(iRow).Label = sAnswerName
                    serPrice(iRow).IsValueShownAsLabel = True
                    serPrice(iRow).AxisLabel = ""

                    C.Series.Add(serPrice(iRow))
                End If
                Dim dPrice As New DataPoint
                If dShares = 0 Then dPrice.Color = Color.Red
                dPrice.SetValueY(dShares)
                serPrice(iRow).Points.Add(dPrice)
            End If
        Next
        Return ""
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
            sTitle1 = Replace(sTitle1, "_", " ")

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

    Private Sub lnkURL_LinkClicked(sender As System.Object, e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) Handles lnkURL.LinkClicked
        Process.Start(lnkURL.Text)
    End Sub
End Class