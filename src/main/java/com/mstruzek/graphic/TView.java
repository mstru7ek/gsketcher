package com.mstruzek.graphic;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyAdapter;
import java.awt.event.KeyEvent;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.util.Random;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.JToolBar;
import javax.swing.SwingUtilities;

import com.mstruzek.controller.Controller;
import com.mstruzek.controller.ActiveKey;
import com.mstruzek.msketch.GeometricConstraintType;

public class TView extends JFrame {

    private static final long serialVersionUID = 1L;
    public static final int L_WIDTH = 920;
    public static final int R_HEIGHT = 1400;
    public static final int L_HEIGHT = 1400;
    public static final int R_WIDTH = 1420;


    Container cp = getContentPane();
    /**
     * zmienna na parametry
     */
    final JTextField param = new JTextField(5);

    /**
     * tabelki z wiezami,figurami i parametrami
     */
    MyTables myTables = null;

    /**
     * wypisujemy tutaj wszystko co idzie na System.out.println();
     */
    final JTextArea sytemOutPrintln = new JTextArea(13, 66);
    /**
     * glowny controller
     */
    final Controller controller;


    final MySketch ms;


    public TView(String windowTitle, Controller controll) {
        super(windowTitle);
        this.controller = controll;
        param.setText("10.0");

        cp.setLayout(new BorderLayout());

        setFocusable(true);

        addKeyListener(new KeyAdapter() {
            @Override
            public void keyPressed(KeyEvent e) {
                if (e.getKeyChar() == 'k') {
                    ms.ACK = ActiveKey.K;
                } else if (e.getKeyChar() == 'l') {
                    ms.ACK = ActiveKey.L;
                } else if (e.getKeyChar() == 'm') {
                    ms.ACK = ActiveKey.M;
                } else if (e.getKeyChar() == 'n') {
                    ms.ACK = ActiveKey.N;
                } else {
                    ms.ACK = ActiveKey.None;
                }
                super.keyPressed(e);
            }
        });

        // OGOLNY ROZKLAD
        JPanel all = new JPanel();
        all.setLayout(new BorderLayout());

        JPanel left = new JPanel();
        JPanel right = new JPanel();


        left.setPreferredSize(new Dimension(L_WIDTH, L_HEIGHT));
        // left.setBackground(Color.GREEN);

        right.setPreferredSize(new Dimension(R_WIDTH, R_HEIGHT));

        // right.setBackground(Color.BLUE);

        all.add(left, BorderLayout.WEST);
        all.add(right, BorderLayout.EAST);

        // SZKICOWNIK


        ms = new MySketch(this.controller);
        left.add(ms);

        //FIXME dodatki do szkicownika - Coordinates i Pojemnik na punkty

        // pojemnik na K,L,M,N
        JLabel jl = ms.jl;

        // wyswietla aktualna pozycje kursora
        JLabel currentPosition = ms.currentPosition;

        JPanel panCorPoints = new JPanel();
        panCorPoints.setLayout(new BorderLayout());

        panCorPoints.setBackground(new Color(250, 200, 200));

        panCorPoints.setPreferredSize(new Dimension(920, 50));
        panCorPoints.setBorder(BorderFactory.createLineBorder(Color.black));
        panCorPoints.add(jl, BorderLayout.NORTH);
        panCorPoints.add(currentPosition, BorderLayout.SOUTH);

        left.add(panCorPoints);

        // Dodawanie wiezow

        JPanel constraintMenu = new JPanel();
        constraintMenu.setBackground(new Color(244, 249, 192));
        constraintMenu.setPreferredSize(new Dimension(400, 240));
        constraintMenu.setBorder(BorderFactory
            .createTitledBorder("Add Constraint"));
        final JTextArea opisWiezu = new JTextArea(7, 30);
        opisWiezu.setBorder(BorderFactory.createTitledBorder("HELP"));
        opisWiezu.setLineWrap(true);
        opisWiezu.setWrapStyleWord(true);
        opisWiezu.setEditable(false);
        opisWiezu.setFocusable(false);
        opisWiezu.setBackground(new Color(100, 255, 100, 50));

        final JComboBox combo = new JComboBox(GeometricConstraintType.values());
        combo.setFocusable(false);
        combo.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                JComboBox cb = (JComboBox) e.getSource();
                GeometricConstraintType what = (GeometricConstraintType) cb
                    .getSelectedItem();

                opisWiezu.setText(what.getHelp());
                opisWiezu.getParent().repaint();
                // trzeba pobrac jeszce pobrac dane zmiennej

            }

        });
        JButton addCon = new JButton("Add Constraint");
        addCon.addActionListener(new ActionListener() {

            @Override
            public void actionPerformed(ActionEvent e) {
                GeometricConstraintType what = (GeometricConstraintType) combo
                    .getSelectedItem();
                // System.out.println(what);
                controller.addConstraint(what, ms.mpc.getPointK(), ms.mpc
                        .getPointL(), ms.mpc.getPointM(), ms.mpc.getPointN(),
                    Double.parseDouble(param.getText()));
                ms.mpc.clearAll();
                requestFocus();
            }

        });
        constraintMenu.add(combo);
        constraintMenu.add(opisWiezu);
        constraintMenu.add(param);
        constraintMenu.add(addCon);

        left.add(constraintMenu);

        // Tabelki

        myTables = new MyTables(controller.getConstraintTableModel(),
            controller.getPrimitivesTableModel(), controller
            .getParametersTableModel());
        myTables.setFocusable(false);

        right.add(myTables);
        right.add(new JScrollPane(sytemOutPrintln));
        redirectSystemStreams();


        // FIXME - JToolBar [Open] [Save]

        // ToolBar
        JToolBar jtb = new JToolBar();
        JButton norm = new JButton("Normal");
        JButton dline = new JButton("Draw Line");
        JButton dcircle = new JButton("Draw Circle");
        JButton darc = new JButton("Draw Arc");
        JButton dpoint = new JButton("Draw Point");
        JButton drefresh = new JButton("REFRESH");
        JButton dsolve = new JButton("SOLVE");
        JButton drelax = new JButton("RELAX");
        JButton dfluctuate = new JButton("FLUCTUATE");
        dsolve.setBackground(Color.GREEN);

        norm.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                ms.setStateSketch(MySketchState.Normal);

            }
        });
        dline.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                ms.setStateSketch(MySketchState.DrawLine);
            }
        });

        dcircle.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                ms.setStateSketch(MySketchState.DrawCircle);

            }
        });

        darc.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                ms.setStateSketch(MySketchState.DrawArc);

            }
        });
        dpoint.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                ms.setStateSketch(MySketchState.DrawPoint);

            }
        });
        drefresh.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                ms.refreshContainers();
                ms.repaint();
            }
        });
        dsolve.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                controller.solveSystem();
                ms.refreshContainers();
                ms.repaint();
            }
        });
        drelax.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                controller.relaxForces();
                ms.refreshContainers();
                ms.repaint();
            }
        });

        dfluctuate.addActionListener(new ActionListener() {
            final Random random = new Random();

            @Override
            public void actionPerformed(ActionEvent e) {
                double coefficient = random.nextDouble() * 20;
                controller.fluctuatePoints(coefficient);
                ms.refreshContainers();
                ms.repaint();
            }
        });
        // FIXME - wazne dla setFocusable
        norm.setFocusable(false);
        dline.setFocusable(false);
        dcircle.setFocusable(false);
        darc.setFocusable(false);
        dpoint.setFocusable(false);
        drefresh.setFocusable(false);
        dsolve.setFocusable(false);
        drelax.setFocusable(false);
        dfluctuate.setFocusable(false);

        jtb.add(norm);
        jtb.add(dline);
        jtb.add(dcircle);
        jtb.add(darc);
        jtb.add(dpoint);
        jtb.addSeparator(new Dimension(20, 1));
        jtb.add(drefresh);
        jtb.addSeparator(new Dimension(20, 1));
        jtb.add(dsolve);
        jtb.add(drelax);
        jtb.add(dfluctuate);

        // GLOWNY ROZKLAD TOOLBAR I OKNO
        cp.add(jtb, BorderLayout.NORTH);
        cp.add(all, BorderLayout.CENTER);

        // KONIEC
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        addKeyListener(ms.getMyKeyListener());

        pack();
        setVisible(true);

    }

    private void updateTextArea(final String text) {
        SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                sytemOutPrintln.append(text);
            }
        });
    }

    private void redirectSystemStreams() {
        OutputStream out = new OutputStream() {
            @Override
            public void write(int b) throws IOException {
                updateTextArea(String.valueOf((char) b));
            }

            @Override
            public void write(byte[] b, int off, int len) throws IOException {
                updateTextArea(new String(b, off, len));
            }

            @Override
            public void write(byte[] b) throws IOException {
                write(b, 0, b.length);
            }
        };

        System.setOut(new PrintStream(out, true));
        System.setErr(new PrintStream(out, true));
    }

}
