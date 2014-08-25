Imports System.Windows.Forms

Imports Message = OpenPop.Mime.Message

Namespace Gridcoin

    ''' <summary>
    ''' Builds up a <see cref="TreeNode"/> with the same hierarchy as
    ''' a <see cref="Message"/>.
    ''' 
    ''' The root <see cref="TreeNode"/> has the subject as text and has one one child.
    ''' The root has no Tag attribute set.
    ''' 
    ''' The children of the root has the MediaType of the <see cref="MessagePart"/> as text and the
    ''' MessagePart's children as <see cref="TreeNode"/> children.
    ''' Each <see cref="MessagePart"/> <see cref="TreeNode"/> has a Tag property set to that <see cref="MessagePart"/>
    ''' </summary>
    Friend Class TreeNodeBuilder
        Implements IAnswerMessageTraverser(Of TreeNode)
        Public Function VisitMessage(ByVal message As OpenPop.Mime.Message) As TreeNode Implements IAnswerMessageTraverser(Of TreeNode).VisitMessage
            If message Is Nothing Then
                Throw New ArgumentNullException("message")
            End If
            ' First build up the child TreeNode
            '  Dim child As TreeNode = VisitMessagePart(message.MessagePart)
            ' Then create the topmost root node with the subject as text
            'Dim child As New TreeNode()
            Dim children(0) As TreeNode
            '   children(0) = child
            'Dim topNode As New TreeNode(message.Headers.Subject, children)
            Dim topNode As New TreeNode(message.Headers.Subject)

            Return topNode
        End Function

        Public Function VisitMessagePart(ByVal messagePart As OpenPop.Mime.MessagePart) As TreeNode
            If messagePart Is Nothing Then
                Throw New ArgumentNullException("messagePart")
            End If

            ' Default is that this MessagePart TreeNode has no children
            Dim children As TreeNode() = New TreeNode(-1) {}

            If messagePart.IsMultiPart And 1 = 0 Then
                ' If the MessagePart has children, in which case it is a MultiPart, then
                ' we create the child TreeNodes here
                children = New TreeNode(messagePart.MessageParts.Count - 1) {}

                For i As Integer = 0 To messagePart.MessageParts.Count - 1
                    children(i) = VisitMessagePart(messagePart.MessageParts(i))
                Next
            End If

            If 1 = 0 Then
                ' Create the current MessagePart TreeNode with the found children
                Dim currentNode As New TreeNode(messagePart.ContentType.MediaType, children)

                ' Set the Tag attribute to point to the MessagePart.
                currentNode.Tag = messagePart

                Return currentNode
            End If

        End Function

        Public Function VisitMessagePart1(ByVal messagePart As OpenPop.Mime.MessagePart) As System.Windows.Forms.TreeNode Implements IAnswerMessageTraverser(Of System.Windows.Forms.TreeNode).VisitMessagePart

        End Function
    End Class
End Namespace


Namespace Gridcoin

    ''' <summary>
    ''' This interface describes a MessageTraverser which is able to traverse a Message hierarchy structure
    ''' and deliver some answer.
    ''' </summary>
    ''' <typeparam name="TAnswer">This is the type of the answer you want to have delivered.</typeparam>
    Public Interface IAnswerMessageTraverser(Of TAnswer)
        ''' <summary>
        ''' Call this when you want to apply this traverser on a <see cref="Message"/>.
        ''' </summary>
        ''' <param name="message">The <see cref="Message"/> which you want to traverse. Must not be <see langword="null"/>.</param>
        ''' <returns>An answer</returns>
        Function VisitMessage(ByVal message As Message) As TAnswer

        ''' <summary>
        ''' Call this when you want to apply this traverser on a <see cref="MessagePart"/>.
        ''' </summary>
        ''' <param name="messagePart">The <see cref="MessagePart"/> which you want to traverse. Must not be <see langword="null"/>.</param>
        ''' <returns>An answer</returns>
        Function VisitMessagePart(ByVal messagePart As OpenPop.Mime.MessagePart) As TAnswer
    End Interface
End Namespace


