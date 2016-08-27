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
        Dim sAnswersUsed As String = ""
        Dim iRow As Long = 0
        C.Titles.Clear() : C.Titles.Add("Poll Results " + sTitle) : C.BackColor = Color.Black : C.ForeColor = Color.Lime
        C.Titles(0).Alignment = ContentAlignment.TopRight
        C.Titles(0).Font = New System.Drawing.Font("Arial", 18)


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


        C.ChartAreas(0).AxisX.IsLabelAutoFit = True
        C.ChartAreas(0).AxisX.LabelAutoFitStyle = LabelAutoFitStyles.LabelsAngleStep30
        C.ChartAreas(0).AxisX.LabelStyle.Enabled = True


        'Create series for each answer
        Dim serPrice(vAnswers.Length - 1) As Series
        serPrice(iRow) = New Series
        serPrice(iRow).ChartType = SeriesChartType.Pie
        serPrice(iRow).IsVisibleInLegend = True
        serPrice(iRow).IsValueShownAsLabel = True
        serPrice(iRow).AxisLabel = ""
        serPrice(iRow).SmartLabelStyle.IsMarkerOverlappingAllowed = False

        serPrice(iRow).Font = New System.Drawing.Font("Arial", 14)

        C.Series.Add(serPrice(iRow))
        Dim dHiVote As Double = 0
        For subY As Integer = 0 To vAnswers.Length - 1
            If Len(vAnswers(subY)) > 0 Then
                Dim dShares As String = CDbl(ExtractXML(vAnswers(subY), "<SHARES>"))
                If dShares > dHiVote Then dHiVote = dShares
            End If
        Next subY
        Dim dMinShares As Double = dHiVote * 0.01
        If dMinShares < 1 Then dMinShares = 1


        For subY As Integer = 0 To vAnswers.Length - 1
            If Len(vAnswers(subY)) > 0 Then
                iRow += 1
                Dim sAnswerName As String = ExtractXML(vAnswers(subY), "<ANSWERNAME>")
                If sAnswersUsed.Contains(sAnswerName) Then
                    sAnswerName += "_2"
                End If
                sAnswersUsed += sAnswerName
                Dim sParticipants As String = ExtractXML(vAnswers(subY), "<PARTICIPANTS>")
                Dim dShares As String = CDbl(ExtractXML(vAnswers(subY), "<SHARES>"))
                If dShares = 0 Then dShares = dMinShares
          
                Dim dpPrice As New DataPoint
                If dShares = 0 Then dpPrice.Color = Color.Red
                If Len(sAnswerName) > 14 Then
                    dpPrice.Font = New System.Drawing.Font("Arial", 7)
                End If
                dpPrice.LegendText = sAnswerName
                If dShares > dMinShares Then
                    dpPrice.Label = sAnswerName
                Else
                    dpPrice.IsValueShownAsLabel = False
                    dpPrice.Label = ""
                End If

                dpPrice.LabelForeColor = Color.DarkGreen

                dpPrice.SetValueY(dShares)
                serPrice(0).Points.Add(dpPrice)

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