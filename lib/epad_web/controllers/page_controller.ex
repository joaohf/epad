defmodule EpadWeb.PageController do
  use EpadWeb, :controller

  def index(conn, _params) do
    conn
    |> put_layout("app.html")
    |> render("index.html")
  end
end
