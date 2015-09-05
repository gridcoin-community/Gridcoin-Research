Imports System.Windows.Forms.DataVisualization
Imports System.Windows.Forms.DataVisualization.Charting
Imports System.Drawing
Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmFaqAnswerView


    Public _id As String
    Public _QuestionID As String

    Public mbEmpty As Boolean
    Private votedRowId As Long = 0
    Private bHandlersLoaded As Boolean = False

    Private Sub frmFaqAnswerView_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
        If mbEmpty Then Me.Hide() : Exit Sub

    End Sub

    
    Private Sub frmFaqAnswerView_KeyDown(sender As Object, e As System.Windows.Forms.KeyEventArgs) Handles Me.KeyDown
        If e.KeyCode = Keys.Escape Then
            Me.Hide()
        End If
    End Sub

    Private Sub EnsureRowIsSelected(x As Long, y As Long)
        'if (e.Button != System.Windows.Forms.MouseButtons.Right) { return; }
        Dim pt As System.Drawing.Point = dgv.PointToClient(Cursor.Position)

        Dim rowIndex As Long = dgv.HitTest(pt.X, pt.Y).RowIndex
        If (rowIndex = -1) Then Exit Sub
        dgv.ClearSelection()
        dgv.Rows(rowIndex).Selected = True

    End Sub

    Private Sub DGV_CellClick(sender As Object, e As DataGridViewCellEventArgs)
        Try
         
            mRowIndex = e.RowIndex

            If e.ColumnIndex >= 3 Then
                Call EnsureRowIsSelected(0, 0)
                Dim sId As String = dgv.Rows(e.RowIndex).Tag
                If Len(sId) > 0 Then
                    If e.ColumnIndex = dgv.Columns("UpVote").Index Then
                        mGRCData.InsertUpDownVote(_id, sId, GetSessionGuid, 1)
                        votedRowId = e.RowIndex
                        Call frmFaqAnswerView_Load(Me, Nothing)
                        mfrmFAQ.RefreshFAQ()

                    End If
                    If e.ColumnIndex = dgv.Columns("DownVote").Index Then
                        mGRCData.InsertUpDownVote(_id, sId, GetSessionGuid, -1)
                        votedRowId = e.RowIndex
                        Call frmFaqAnswerView_Load(Me, Nothing)
                        mfrmFAQ.RefreshFAQ()

                    End If
                End If

            End If
        Catch ex As Exception
            MsgBox("You must log in to vote.", MsgBoxStyle.Critical)

            If mfrmLogin Is Nothing Then
                mfrmLogin = New frmLogin
            End If
            mfrmLogin.Show()

        End Try

    End Sub
    Public Function Nice(sData As String) As String
        Dim iChar As Long = 0
        Dim sOut As String = ""
        For x = 1 To Len(sData)
            iChar += 1
            Dim sMid As String = Mid(sData, x, 1)
            If sMid = Chr(13) Or sMid = Chr(10) Then
                iChar = 0
            End If
            If iChar > 148 And sMid = " " Then
                sOut += vbCrLf
                iChar = 0
            End If
            sOut += sMid
        Next
        Return sOut
    End Function
    Private Sub frmFaqAnswerView_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        If mGRCData Is Nothing Then mGRCData = New GRCSec.GridcoinData

        'List the Answers
        Dim sHeading As String = "#;Answer;Added"

        dgv.Rows.Clear()
        dgv.Columns.Clear()
        dgv.SelectionMode = DataGridViewSelectionMode.FullRowSelect
        dgv.EditingPanel.Visible = False
        dgv.AllowUserToAddRows = False
        dgv.ReadOnly = True

        Dim vHeading() As String = Split(sHeading, ";")
        AddHeading(0, "#", dgv, True)
        Dim GRCRTB As New GridcoinRichUI.DataGridViewRichTextBoxColumn
        GRCRTB.Name = "Answer"
        dgv.Columns.Insert(1, GRCRTB)
        AddHeading(2, "Added", dgv, True)

        Dim dgvbc As New DataGridViewButtonColumn
        dgvbc.Name = "UpVote"
        dgvbc.Text = "Up Vote"
        dgvbc.UseColumnTextForButtonValue = False
        dgv.Columns.Insert(3, dgvbc)
        Dim dgvbcdv As New DataGridViewButtonColumn
        dgvbcdv.Name = "DownVote"
        dgvbcdv.Text = "Down Vote"
        dgv.Columns.Insert(4, dgvbcdv)
        If Not bHandlersLoaded Then
            AddHandler dgv.CellClick, AddressOf DGV_CellClick
            bHandlersLoaded = True
        End If
        Dim iRow As Long = 0
        Dim dr As SqlDataReader
        dr = mGRCData.mGetFAQAnswers(_id)
        If dr Is Nothing Then mbEmpty = True : Exit Sub

        dgv.Columns(1).MinimumWidth = 800
        lblTitle.Text = ""
        Try
            While dr.Read
                If iRow = 0 Then
                    lblTitle.Text = dr("Question")
                End If
                dgv.Rows.Add()
                dgv.Rows(iRow).Cells(0).Value = iRow + 1
                dgv.Rows(iRow).Cells(1).Value = Nice(dr("Answer"))
                Dim h As Long = Len(dr("answer")) / 3
                If h < 50 Then h = 50
                dgv.Rows(iRow).Height = h
                dgv.Rows(iRow).Cells(2).Value = Trim(dr("Added")) 'Answer added date
                'dgv.Rows(iRow).Cells(1).for()
                dgv.Rows(iRow).Cells(1).Style.ForeColor = Drawing.Color.LightGreen
                
                'Upvote button
                dgv.Rows(iRow).Cells(3).Value = "(" + Trim(dr("UpVotes")) + ") - Up Vote"
                'Downvote button
                dgv.Rows(iRow).Cells(4).Value = "(" + Trim(dr("downvotes")) + ") - Down Vote"
                dgv.Rows(iRow).Tag = dr("id").ToString()
                dgv.Rows(iRow).DefaultCellStyle.ForeColor = Color.Yellow

                dgv.Rows(iRow).DefaultCellStyle.SelectionBackColor = Color.FromArgb(&H49, &H3D, &H26) 'mocha
                dgv.Rows(iRow).DefaultCellStyle.SelectionForeColor = Color.Yellow
                iRow += 1
            End While
            If votedRowId > 0 Then dgv.Rows(votedRowId).Selected = True
            EnsureRowIsSelected(0, 0)
            If iRow = 0 Then mbEmpty = True

        Catch ex As Exception
            Log(ex.Message)

        End Try

    End Sub
End Class